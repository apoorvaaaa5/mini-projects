#include <scheduler.h>
#include <mmio.h>
#include <usb_cdc.h>
#include <arch.h>
#include <string.h>

/* ARM Cortex-M0+ SysTick Hardware Registers (Private Peripheral Bus) */
#define SYST_CSR            0xE000E010  /* Control and Status Register */
#define SYST_RVR            0xE000E014  /* Reload Value Register */
#define SYST_CVR            0xE000E018  /* Current Value Register */

#define SYST_CSR_ENABLE     (1 << 0)    /* Counter Enable */
#define SYST_CSR_TICKINT    (1 << 1)    /* SysTick Exception Request Enable */
#define SYST_CSR_CLKSOURCE  (1 << 2)    /* 1 = clk_sys (125 MHz), 0 = 1 MHz ref */

/* Variables from user template */
uint8_t R = 0, currentTaskId = 0, nextTaskId = 0, num_tasks = 0;
tcb task[MAX_TASKS];

/* Internal static stack pool managed completely by scheduler */
static uint32_t task_stacks[MAX_TASKS][STACK_SIZE_WORDS];

/* 32-bit monotonic process identifier counter (increments 1, 2, 3...) */
static uint32_t next_pid = 1;

volatile uint32_t system_millis = 0;

static void task_exit_guard(void)
{
	/* Auto-reap task when its function finishes and returns */
	task_kill(currentTaskId);
	while (1) {
		asm volatile("wfi");
	}
}

/* Helper to setup initial stack frame for a task */
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

int task_create_named(const char *name, void (*entry)(void), task_mode_t mode)
{
	if (!entry) return -1;

	/* Critical section: prevent SysTick preemption while updating task table */
	arch_di();

	int target_id = -1;
	uint32_t assigned_pid = 0;

	if (num_tasks == 0) {
		/* Slot 0 is reserved strictly for root shell task (PID 0) */
		target_id = 0;
		assigned_pid = 0;
		num_tasks = 1;
	} else {
		/* Lowest-Index First-Fit: scan from slot 1 upwards for first free slot */
		for (int i = 1; i < MAX_TASKS; i++) {
			if (task[i].state == t_quit || task[i].name[0] == '\0') {
				target_id = i;
				break;
			}
		}

		/* No free slots available in the bounded pool */
		if (target_id == -1) {
			arch_ei();
			return -1;
		}

		/* Assign fresh 32-bit monotonic PID */
		assigned_pid = next_pid++;
		if (next_pid == 0) next_pid = 1; /* Skip 0 if 32-bit counter ever wraps */
	}

	uint8_t id = (uint8_t)target_id;
	task[id].slot_id = id;
	task[id].pid = assigned_pid;
	task[id].fnctpt = entry;
	task[id].priority = 0;
	task[id].mode = mode;

	if (name) {
		strncpy(task[id].name, name, sizeof(task[id].name) - 1);
		task[id].name[sizeof(task[id].name) - 1] = '\0';
	} else {
		task[id].name[0] = '\0';
	}

	/* Manage stack */
	uint32_t stack_top = (uint32_t)(&task_stacks[id][STACK_SIZE_WORDS]) & ~0x7UL;
	task[id].stack_pointer_begin = stack_top;

	if (id == 0) {
		/* Task 0 (Shell) will be launched directly with a clean stack */
		task[id].stack_pointer_end = stack_top;
	} else {
		/* Synthesize stack frame BEFORE announcing ready to scheduler */
		task[id].stack_pointer_end = init_task_stack(stack_top, entry);
	}

	/* CONCURRENCY SAFE: State is set to ready as the absolute last step */
	task[id].state = t_ready;

	arch_ei();

	return (int)assigned_pid;
}

int task_create(void (*entry)(void))
{
	return task_create_named("task", entry, MODE_BACKGROUND);
}

tcb *task_get_by_pid(uint32_t pid)
{
	if (pid == 0) {
		if (task[0].state != t_quit && task[0].name[0] != '\0') return &task[0];
		return 0;
	}
	for (int i = 1; i < MAX_TASKS; i++) {
		if (task[i].pid == pid && task[i].state != t_quit && task[i].name[0] != '\0') {
			return &task[i];
		}
	}
	return 0;
}

int task_kill(uint8_t slot_id)
{
	if (slot_id == 0 || slot_id >= MAX_TASKS) {
		return -1; /* Cannot kill shell or invalid slot ID */
	}
	if (task[slot_id].state == t_quit && task[slot_id].name[0] == '\0') {
		return -1; /* Already quit */
	}

	arch_di();
	task[slot_id].state = t_quit;
	task[slot_id].name[0] = '\0'; /* Clean slot wipe */
	task[slot_id].pid = 0;
	arch_ei();

	return 0;
}

int task_kill_by_name(const char *name)
{
	if (!name || strcmp(name, "shell") == 0) {
		return -1;
	}
	for (int i = 1; i < MAX_TASKS; i++) {
		if (strcmp(task[i].name, name) == 0 && task[i].state != t_quit) {
			return task_kill(i);
		}
	}
	return -1;
}

void timer_init(void)
{
	/* Disable SysTick during configuration */
	mmio32(SYST_CSR) = 0;

	/* Set reload value for 1 ms tick using 1 MHz reference clock:
	 * 1 MHz clock -> 1,000 cycles per ms.
	 * Reload register takes N - 1 = 999.
	 */
	mmio32(SYST_RVR) = 999;

	/* Clear current counter */
	mmio32(SYST_CVR) = 0;

	/* Enable counter and interrupt using 1 MHz reference clock (CLKSOURCE = 0) */
	//// Set this to 1mhz clock.
	mmio32(SYST_CSR) = SYST_CSR_ENABLE | SYST_CSR_TICKINT;
}

void scheduler_start(void)
{
	if (num_tasks == 0) {
		return;
	}

	/* Explicitly ensure all unused app slots are marked quit and empty */
	for (int i = 1; i < MAX_TASKS; i++) {
		if (task[i].state != t_busy && task[i].state != t_ready) {
			task[i].state = t_quit;
			task[i].name[0] = '\0';
			task[i].pid = 0;
		}
	}

	// setting up first task parameters
	currentTaskId = task[0].slot_id;
	if (task[0].state == t_ready)
		task[0].state = t_busy;

	arch_ei();	// sei();

	// running first task directly with clean stack
	start_first_task(task[0].stack_pointer_begin, task[0].fnctpt);
}

/*
 * C-level scheduler called on each SysTick tick:
 * Selects next active task in round-robin fashion, skipping t_quit tasks
 */
uint32_t schedule_next(uint32_t current_sp)
{
	system_millis++;

	// getting the termination pointer of individual stacks
	task[currentTaskId].stack_pointer_end = current_sp;

	// find next runnable task (state == t_busy or t_ready)
	for (uint8_t i = 0; i < MAX_TASKS; i++) {
		if (nextTaskId >= MAX_TASKS) nextTaskId = 0;
		if (task[nextTaskId].state == t_busy || task[nextTaskId].state == t_ready) {
			task[nextTaskId].state = t_busy;
			currentTaskId = nextTaskId;
			nextTaskId = (nextTaskId + 1) % MAX_TASKS;
			return task[currentTaskId].stack_pointer_end;
		}
		nextTaskId++;
	}

	/* Fallback: keep running current task */
	return task[currentTaskId].stack_pointer_end;
}

void mdelay(uint32_t ms)
{
	uint32_t start = system_millis;
	while ((system_millis - start) < ms) {
		usb_poll();
	}
}
