#include <platform.h>
#include <usb_cdc.h>
#include <console.h>
#include <pwm.h>
#include <arch.h>
#include <heap.h>
#include <systick.h>
#include <scheduler.h>
#include <shell.h>

int main(void)
{
	/* 1. Hardware bootstrap (Clocks, GPIO, USB CDC) */
	bootstrap();

	/* 2. Initialize 16 KB Dynamic Heap Allocator */
	heap_init();

	/* 3. Initialize Hardware PWM Engine (2 kHz on GPIO 25 onboard LED) */
	pwm_init();

	arch_di();	// cli();

	/* 4. Initialize ARM Cortex-M0+ SysTick Timer (1 ms periodic quantum) */
	systick_init();

	/* 5. Dynamically allocate and spawn primary interactive Shell Task (PID 0) */
	task_create_named("shell", shell_task);

	/* 6. Start the round-robin multitasking engine */
	scheduler_start();

	/* Unreachable */
	while (1) {
		arch_wfi();
	}

	return 0;
}
