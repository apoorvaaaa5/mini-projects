#include <scheduler.h>
#include <mmio.h>
#include <usb_cdc.h>
#include <arch.h>

/* ARM Cortex-M0+ SysTick Hardware Registers (Private Peripheral Bus) */
#define SYST_CSR            0xE000E010  /* Control and Status Register */
#define SYST_RVR            0xE000E014  /* Reload Value Register */
#define SYST_CVR            0xE000E018  /* Current Value Register */

#define SYST_CSR_ENABLE     (1 << 0)    /* Counter Enable */
#define SYST_CSR_TICKINT    (1 << 1)    /* SysTick Exception Request Enable */
#define SYST_CSR_CLKSOURCE  (1 << 2)    /* 1 = clk_sys (125 MHz), 0 = 1 MHz ref */

/* Variables from user template */
uint8_t R = 0, currentTaskId = 0, nextTaskId = 0, num_tasks = 0;
tcb task[maxTask];

/* Internal static stack pool managed completely by scheduler */
static uint32_t task_stacks[maxTask][STACK_SIZE_WORDS];

volatile uint32_t system_millis = 0;

static void task_exit_guard(void)
{
	/* Safety loop if a task ever exits */
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
	sp[15] = 0x01000000UL;              /* xPSR (Thumb bit 24 = 1) */
	sp[14] = (uint32_t)entry;           /* PC (Task entry function) */
	sp[13] = (uint32_t)task_exit_guard; /* LR (Fallback exit guard) */
	sp[12] = 0;                         /* R12 */
	sp[11] = 0;                         /* R3 */
	sp[10] = 0;                         /* R2 */
	sp[9]  = 0;                         /* R1 */
	sp[8]  = 0;                         /* R0 */

	/* Software stacked frame (R4 - R11) */
	sp[7] = sp[6] = sp[5] = sp[4] = 0;  /* R11, R10, R9, R8 */
	sp[3] = sp[2] = sp[1] = sp[0] = 0;  /* R7, R6, R5, R4 */

	return (uint32_t)sp;
}

int task_create(void (*entry)(void))
{
	if (num_tasks >= maxTask || entry == 0) {
		return -1;
	}

	uint8_t id = num_tasks;

	task[id].task_id = id;
	task[id].fnctpt = entry;
	task[id].priority = 0;
	task[id].state = t_ready;

	/* Internally manage stack allocation from private static pool */
	uint32_t stack_top = (uint32_t)(&task_stacks[id][STACK_SIZE_WORDS]) & ~0x7UL;
	task[id].stack_pointer_begin = stack_top;

	if (id == 0) {
		/* Task 0 will be launched directly with a clean stack */
		task[id].stack_pointer_end = stack_top;
	} else {
		/* Other tasks require pre-synthesized stack frame for exception return */
		task[id].stack_pointer_end = init_task_stack(stack_top, entry);
	}

	num_tasks++;
	return id;
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

	// setting up first task parameters
	currentTaskId = task[0].task_id;
	if (task[0].state == t_ready)
		task[0].state = t_busy;

	arch_ei();	// sei();

	// running first task directly with clean stack
	start_first_task(task[0].stack_pointer_begin, task[0].fnctpt);
}

/*
 * C-level scheduler called on each SysTick tick:
 * Mirrors the logic from template's ISR(TIMER0_COMPA_vect)
 */
uint32_t schedule_next(uint32_t current_sp)
{
	system_millis++;

	// getting the termination pointer of individual stacks
	task[currentTaskId].stack_pointer_end = current_sp;

	// checks if all the tasks are in queue / round robin selection
	if (nextTaskId >= num_tasks) nextTaskId = 0;

	if (task[nextTaskId].state == t_busy || task[nextTaskId].state == t_ready)
	{
		// pointing stack to start context restoring
		task[nextTaskId].state = t_busy;
		currentTaskId = nextTaskId;
		nextTaskId = task[nextTaskId].task_id + 1;
	}

	return task[currentTaskId].stack_pointer_end;
}

void mdelay(uint32_t ms)
{
	uint32_t start = system_millis;
	while ((system_millis - start) < ms) {
		usb_poll();
	}
}
