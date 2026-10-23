#include <platform.h>
#include <usb_cdc.h>
#include <console.h>
#include <pwm.h>
#include <arch.h>
#include <scheduler.h>

#define STEP_DELAY_MS  10  /* 10 ms per PWM step -> smooth breathing effect */

/*
 * Task 1: Terminal Print Task
 * Updates rotating spinner over USB CDC serial monitor every 1 second
 */
void task1_blinky_print(void)
{
	static char prog[] = "-\\|/";
	uint32_t seconds = 0;
	int was_connected = 0;

	while (1) {
		if (usb_cdc_is_connected()) {
			if (!was_connected) {
				was_connected = 1;
				console_puts("\r\n========================================\r\n");
				console_puts("RP2040 Bare-Metal Round-Robin Scheduler\r\n");
				console_puts("Task 1: Terminal Print | Task 2: Breathe LED\r\n");
				console_puts("========================================\r\n");
			}
			console_puts("\rRunning Blinky...[");
			console_putc(prog[seconds % 4]);
			console_puts("]");
		}
		seconds++;
		mdelay(1000);
	}
}

/*
 * Task 2: Breathing LED Task
 * Modulates hardware PWM duty cycle (0% -> 100% -> 0%) on GPIO 25
 */
void task2_breathe_led(void)
{
	task_create(task1_blinky_print);
	while (1) {
		/* Inhale: Ramp duty cycle up (0% -> 100%) */
		for (int i = 0; i <= 100; i++) {
			pwm_set(i);
			mdelay(STEP_DELAY_MS);
		}

		/* Exhale: Ramp duty cycle down (100% -> 0%) */
		for (int i = 100; i >= 0; i--) {
			pwm_set(i);
			mdelay(STEP_DELAY_MS);
		}
	}
}

int main(void)
{
	/* 1. Hardware bootstrap (Clocks, GPIO, USB CDC) */
	bootstrap();

	/* 2. Initialize Hardware PWM Engine (2 kHz on GPIO 25 onboard LED) */
	pwm_init();

	arch_di();	// cli();

	/* 3. Initialize SysTick timer */
	timer_init();

	/* 4. Create and initialize tasks via API (stack allocated internally, maxTask = 4) */
	task_create(task2_breathe_led);

	/* 5. Start the round-robin scheduler */
	scheduler_start();

	while (1);
	return 0;
}
