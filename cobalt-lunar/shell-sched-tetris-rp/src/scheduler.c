#include <scheduler.h>
#include <heap.h>
#include <usb_cdc.h>
#include <arch.h>
#include <string.h>

/* Global scheduler state */
tcb *current_task = NULL;
static tcb *ready_queue_head = NULL;
static tcb *ready_queue_tail = NULL;

/* 32-bit monotonic process identifier counter (0 = shell, 1, 2, 3...) */
static uint32_t next_pid = 0;
static uint32_t active_task_count = 0;

volatile uint32_t system_millis = 0;

/* Helper to setup initial stack frame for a task */
static uint32_t init_task_stack(uint32_t stack_top_addr, void (*entry)(void));

/*
 * Internal Queue Operations:
 * Fully encapsulated within scheduler.c — Users/Shell never touch queue pointers directly.
 */
static void queue_insert(tcb *node)
{
	if (node == NULL) return;

	if (ready_queue_head == NULL) {
		ready_queue_head = node;
		ready_queue_tail = node;
		node->next = node;
	} else {
		node->next = ready_queue_head;
		ready_queue_tail->next = node;
		ready_queue_tail = node;
	}
	active_task_count++;
}

static void queue_remove(tcb *node)
{
	if (node == NULL || ready_queue_head == NULL) return;

	if (node->next == node) {
		/* Only 1 node in circular queue */
		ready_queue_head = NULL;
		ready_queue_tail = NULL;
	} else {
		/* Find predecessor in circular singly-linked list */
		tcb *prev = ready_queue_head;
		while (prev->next != node && prev->next != ready_queue_head) {
			prev = prev->next;
		}
		if (prev->next == node) {
			prev->next = node->next;
			if (ready_queue_head == node) {
				ready_queue_head = node->next;
			}
			if (ready_queue_tail == node) {
				ready_queue_tail = prev;
			}
		}
	}
	node->next = NULL;
	if (active_task_count > 0) {
		active_task_count--;
	}
}

static void task_exit_guard(void)
{
	/* Auto-terminate task when its function returns */
	if (current_task != NULL) {
		current_task->state = t_quit;
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
	sp[15] = 0x01000000UL;                      /* xPSR (Thumb bit 24 = 1) */
	sp[14] = ((uint32_t)entry) | 1UL;           /* PC (Task entry function in Thumb mode) */
	sp[13] = ((uint32_t)task_exit_guard) | 1UL; /* LR (Fallback exit guard in Thumb mode) */
	sp[12] = 0;                                 /* R12 */
	sp[11] = 0;                                 /* R3 */
	sp[10] = 0;                                 /* R2 */
	sp[9]  = 0;                                 /* R1 */
	sp[8]  = 0;                                 /* R0 */

	/* Software stacked frame (R4 - R11) */
	sp[7] = sp[6] = sp[5] = sp[4] = 0;          /* R11, R10, R9, R8 */
	sp[3] = sp[2] = sp[1] = sp[0] = 0;          /* R7, R6, R5, R4 */

	return (uint32_t)sp;
}

int task_create_dynamic(const char *name, void (*entry)(void), uint32_t stack_bytes)
{
	if (entry == NULL) return -1;
	if (stack_bytes < 256) stack_bytes = 256;

	/* 1. Allocate TCB + Stack together in a single contiguous block */
	size_t aligned_tcb_size = (sizeof(tcb) + 7) & ~7UL;
	size_t total_alloc = aligned_tcb_size + stack_bytes;

	tcb *new_task = (tcb *)malloc(total_alloc);
	if (new_task == NULL) {
		return -1; /* Heap exhausted */
	}

	arch_di();

	uint32_t assigned_pid = next_pid++;
	new_task->pid = assigned_pid;
	new_task->fnctpt = entry;
	new_task->next = NULL;

	if (name != NULL) {
		strncpy(new_task->name, name, sizeof(new_task->name) - 1);
		new_task->name[sizeof(new_task->name) - 1] = '\0';
	} else {
		new_task->name[0] = '\0';
	}

	uint32_t stack_top = ((uint32_t)new_task + total_alloc) & ~0x7UL;

	if (assigned_pid == 0) {
		/* Root shell task starts directly with clean stack */
		new_task->stack_pointer_end = stack_top;
		new_task->state = t_busy;
	} else {
		/* Child tasks start with synthesized exception stack frame */
		new_task->stack_pointer_end = init_task_stack(stack_top, entry);
		new_task->state = t_ready;
	}

	/* 2. Scheduler enqueues task into circular singly-linked list */
	queue_insert(new_task);

	arch_ei();

	return (int)assigned_pid;
}

int task_create_named(const char *name, void (*entry)(void))
{
	return task_create_dynamic(name, entry, DEFAULT_STACK_SIZE_BYTES);
}

tcb *task_get_by_pid(uint32_t pid)
{
	if (ready_queue_head == NULL) return NULL;

	arch_di();
	tcb *curr = ready_queue_head;
	tcb *found = NULL;
	do {
		if (curr->pid == pid && curr->state != t_quit) {
			found = curr;
			break;
		}
		curr = curr->next;
	} while (curr != ready_queue_head && curr != NULL);

	arch_ei();
	return found;
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
	if (pid == 0 || ready_queue_head == NULL) {
		return -1;
	}

	arch_di();

	tcb *curr = ready_queue_head;
	tcb *target = NULL;
	do {
		if (curr->pid == pid) {
			target = curr;
			break;
		}
		curr = curr->next;
	} while (curr != ready_queue_head && curr != NULL);

	if (target == NULL || target->state == t_quit) {
		arch_ei();
		return -1;
	}

	if (target == current_task) {
		/* Current task marked quit; schedule_next() unlinks and frees on next tick */
		target->state = t_quit;
	} else {
		/* Inactive task: scheduler unlinks and immediately deallocates memory */
		queue_remove(target);
		free(target);
	}

	arch_ei();
	return 0;
}

int task_kill_by_name(const char *name)
{
	if (name == NULL || strcmp(name, "shell") == 0 || ready_queue_head == NULL) {
		return -1;
	}

	arch_di();
	tcb *curr = ready_queue_head;
	uint32_t target_pid = 0;
	bool found = false;

	do {
		if (strcmp(curr->name, name) == 0 && curr->state != t_quit) {
			target_pid = curr->pid;
			found = true;
			break;
		}
		curr = curr->next;
	} while (curr != ready_queue_head && curr != NULL);

	arch_ei();

	if (found) {
		return task_kill(target_pid);
	}
	return -1;
}

void scheduler_start(void)
{
	if (ready_queue_head == NULL) {
		return;
	}

	current_task = ready_queue_head;
	current_task->state = t_busy;

	arch_ei();

	/* Start executing the root shell task on its pristine dynamic stack */
	start_first_task(current_task->stack_pointer_end, current_task->fnctpt);
}

/*
 * O(1) Constant-Time Round-Robin Scheduler
 * Invoked by pendsv_isr in context_switch.S upon SysTick quantum or task_yield().
 */
uint32_t schedule_next(uint32_t current_sp)
{
	if (current_task == NULL) return current_sp;

	/* 1. Save Stack Pointer of preempted task */
	current_task->stack_pointer_end = current_sp;

	/* 2. Check if current task finished or was killed */
	if (current_task->state == t_quit) {
		tcb *dead_task = current_task;
		tcb *next_task = dead_task->next;

		/* Scheduler unlinks dead task from circular singly-linked list */
		queue_remove(dead_task);

		/* Immediate deallocation of TCB + Stack together in one free() call */
		free(dead_task);

		if (next_task != dead_task && next_task != NULL) {
			current_task = next_task;
		} else {
			current_task = ready_queue_head;
		}
	} else {
		/* O(1) circular step in singly-linked ready queue */
		current_task = current_task->next;
	}

	if (current_task != NULL) {
		current_task->state = t_busy;
		return current_task->stack_pointer_end;
	}

	return current_sp;
}

void task_yield(void)
{
	if (current_task != NULL && active_task_count > 1) {
		/* Trigger PendSV context switch (bit 28 in ICSR 0xE000ED04) */
		*(volatile uint32_t *)0xE000ED04 = (1UL << 28);
		__asm volatile ("isb" : : : "memory");
	}
}

void mdelay(uint32_t ms)
{
	uint32_t start = system_millis;
	while ((system_millis - start) < ms) {
		usb_poll();
		if (current_task != NULL && active_task_count > 1) {
			task_yield();
		}
	}
}
