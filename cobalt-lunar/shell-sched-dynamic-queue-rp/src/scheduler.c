#include <scheduler.h>
#include <heap.h>
#include <usb_cdc.h>
#include <arch.h>
#include <string.h>

/* Global scheduler state */
tcb *current_task = (tcb *)0;
static tcb *ready_queue_head = (tcb *)0;
static tcb *zombie_list = (tcb *)0;

/* 32-bit monotonic process identifier counter (0 = shell, 1, 2, 3...) */
static uint32_t next_pid = 0;
static uint32_t active_task_count = 0;

volatile uint32_t system_millis = 0;

/* Helper to setup initial stack frame for a task */
static uint32_t init_task_stack(uint32_t stack_top_addr, void (*entry)(void));

static void queue_insert(tcb *node)
{
	if (!ready_queue_head) {
		ready_queue_head = node;
		node->next = node;
		node->prev = node;
	} else {
		tcb *tail = ready_queue_head->prev;
		tail->next = node;
		node->prev = tail;
		node->next = ready_queue_head;
		ready_queue_head->prev = node;
	}
	active_task_count++;
}

static void queue_remove(tcb *node)
{
	if (!node || !ready_queue_head) return;

	if (node->next == node) {
		/* Only 1 node in the circular queue */
		ready_queue_head = (tcb *)0;
	} else {
		node->prev->next = node->next;
		node->next->prev = node->prev;
		if (ready_queue_head == node) {
			ready_queue_head = node->next;
		}
	}
	node->next = (tcb *)0;
	node->prev = (tcb *)0;
	if (active_task_count > 0) active_task_count--;
}

void task_reap_zombies(void)
{
	arch_di();
	while (zombie_list) {
		tcb *z = zombie_list;
		zombie_list = z->next;
		if (z->stack_raw) {
			free(z->stack_raw);
		}
		free(z);
	}
	arch_ei();
}

static void task_exit_guard(void)
{
	/* Auto-reap task when its entry function finishes and returns */
	if (current_task) {
		task_kill(current_task->pid);
	}
	while (1) {
		arch_wfi();
	}
}

static uint32_t init_task_stack(uint32_t stack_top_addr, void (*entry)(void))
{
	uint32_t *sp = (uint32_t *)(stack_top_addr & ~0x7UL); /* 8-byte aligned */

	/* Allocate 16 words (64 bytes) for Cortex-M0+ initial context */
	sp -= 16;

	/* Hardware auto-stacked frame */
	sp[15] = 0x01000000UL;                    /* xPSR (Thumb bit 24 = 1) */
	sp[14] = ((uint32_t)entry) | 1UL;           /* PC (Task entry function in Thumb mode) */
	sp[13] = ((uint32_t)task_exit_guard) | 1UL; /* LR (Fallback exit guard in Thumb mode) */
	sp[12] = 0;                               /* R12 */
	sp[11] = 0;                               /* R3 */
	sp[10] = 0;                               /* R2 */
	sp[9]  = 0;                               /* R1 */
	sp[8]  = 0;                               /* R0 */

	/* Software stacked frame (R4 - R11) */
	sp[7] = sp[6] = sp[5] = sp[4] = 0;        /* R11, R10, R9, R8 */
	sp[3] = sp[2] = sp[1] = sp[0] = 0;        /* R7, R6, R5, R4 */

	return (uint32_t)sp;
}

int task_create_dynamic(const char *name, void (*entry)(void), task_mode_t mode, uint32_t stack_bytes)
{
	if (!entry) return -1;
	if (stack_bytes < 256) stack_bytes = 256;

	/* 1. Allocate TCB from Dynamic Heap */
	tcb *new_task = (tcb *)malloc(sizeof(tcb));
	if (!new_task) {
		return -1; /* Heap exhausted */
	}

	/* 2. Allocate Task Stack from Dynamic Heap */
	uint32_t *stack_mem = (uint32_t *)malloc(stack_bytes);
	if (!stack_mem) {
		free(new_task);
		return -1; /* Heap exhausted */
	}

	arch_di();

	uint32_t assigned_pid = next_pid++;
	new_task->pid = assigned_pid;
	new_task->fnctpt = entry;
	new_task->mode = mode;
	new_task->stack_raw = stack_mem;
	new_task->stack_size_bytes = stack_bytes;

	if (name) {
		strncpy(new_task->name, name, sizeof(new_task->name) - 1);
		new_task->name[sizeof(new_task->name) - 1] = '\0';
	} else {
		new_task->name[0] = '\0';
	}

	uint32_t stack_top = ((uint32_t)stack_mem + stack_bytes) & ~0x7UL;
	new_task->stack_pointer_begin = stack_top;

	if (assigned_pid == 0) {
		/* Root shell task starts directly with clean stack */
		new_task->stack_pointer_end = stack_top;
		new_task->state = t_busy;
	} else {
		/* Child tasks start with synthesized exception stack frame */
		new_task->stack_pointer_end = init_task_stack(stack_top, entry);
		new_task->state = t_ready;
	}

	/* 3. Insert into Circular Ready Queue */
	queue_insert(new_task);

	arch_ei();

	return (int)assigned_pid;
}

int task_create_named(const char *name, void (*entry)(void), task_mode_t mode)
{
	return task_create_dynamic(name, entry, mode, DEFAULT_STACK_SIZE_BYTES);
}

tcb *task_get_by_pid(uint32_t pid)
{
	if (!ready_queue_head) return (tcb *)0;

	arch_di();
	tcb *curr = ready_queue_head;
	do {
		if (curr->pid == pid && curr->state != t_quit) {
			arch_ei();
			return curr;
		}
		curr = curr->next;
	} while (curr != ready_queue_head);

	arch_ei();
	return (tcb *)0;
}

tcb *task_get_queue_head(void)
{
	return ready_queue_head;
}

uint32_t task_get_count(void)
{
	return active_task_count;
}

int task_kill(uint32_t pid)
{
	/* Root shell (PID 0) can never be terminated */
	if (pid == 0 || !ready_queue_head) {
		return -1;
	}

	arch_di();

	tcb *curr = ready_queue_head;
	tcb *target = (tcb *)0;
	do {
		if (curr->pid == pid) {
			target = curr;
			break;
		}
		curr = curr->next;
	} while (curr != ready_queue_head);

	if (!target || target->state == t_quit) {
		arch_ei();
		return -1;
	}

	if (target == current_task) {
		/*
		 * Task is attempting to kill itself while running on its own stack!
		 * Mark state as t_quit; schedule_next() will switch stacks and queue
		 * this node into zombie_list for safe deferred deallocation.
		 */
		target->state = t_quit;
	} else {
		/* Task is sleeping or ready; safe to unlink and free immediately */
		queue_remove(target);
		if (target->stack_raw) {
			free(target->stack_raw);
		}
		free(target);
	}

	arch_ei();
	return 0;
}

int task_kill_by_name(const char *name)
{
	if (!name || strcmp(name, "shell") == 0 || !ready_queue_head) {
		return -1;
	}

	arch_di();
	tcb *curr = ready_queue_head;
	uint32_t target_pid = 0;
	int found = 0;

	do {
		if (strcmp(curr->name, name) == 0 && curr->state != t_quit) {
			target_pid = curr->pid;
			found = 1;
			break;
		}
		curr = curr->next;
	} while (curr != ready_queue_head);

	arch_ei();

	if (found) {
		return task_kill(target_pid);
	}
	return -1;
}

void scheduler_start(void)
{
	if (!ready_queue_head) {
		return;
	}

	current_task = ready_queue_head;
	current_task->state = t_busy;

	arch_ei();

	/* Start executing the root shell task on its pristine dynamic stack */
	start_first_task(current_task->stack_pointer_begin, current_task->fnctpt);
}

/*
 * O(1) Constant-Time Round-Robin Scheduler
 * Invoked by systick_isr in context_switch.S every 1 millisecond.
 */
uint32_t schedule_next(uint32_t current_sp)
{
	system_millis++;

	if (!current_task) return current_sp;

	/* 1. Save Stack Pointer of preempted task */
	current_task->stack_pointer_end = current_sp;

	/* 2. Check if current task finished / requested exit */
	if (current_task->state == t_quit) {
		tcb *dead_task = current_task;
		tcb *next_task = dead_task->next;

		queue_remove(dead_task);

		/* Queue into zombie list for safe deferred freeing */
		dead_task->next = zombie_list;
		zombie_list = dead_task;

		if (next_task != dead_task) {
			current_task = next_task;
		} else {
			current_task = ready_queue_head;
		}
	} else {
		/* O(1) circular step to the next task in the ready queue! */
		current_task = current_task->next;
	}

	if (current_task) {
		current_task->state = t_busy;
		return current_task->stack_pointer_end;
	}

	return current_sp;
}

void mdelay(uint32_t ms)
{
	uint32_t start = system_millis;
	while ((system_millis - start) < ms) {
		usb_poll();
		task_reap_zombies();
	}
}
