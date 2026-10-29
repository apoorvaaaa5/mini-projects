#include <platform.h>
#include <usb_cdc.h>
#include <console.h>
#include <pwm.h>
#include <arch.h>
#include <scheduler.h>
#include <shell.h>

#define STEP_DELAY_MS  10  /* 10 ms per PWM step */

/*
 * Task 1: Breathing LED Task (Independent Background Task)
 * Modulates hardware PWM duty cycle (0% -> 100% -> 0%) on GPIO 25
 */
void task_breathe_led(void)
{
	while (1) {
		/* Inhale */
		for (int i = 0; i <= 100; i++) {
			pwm_set(i);
			mdelay(STEP_DELAY_MS);
		}

		/* Exhale */
		for (int i = 100; i >= 0; i--) {
			pwm_set(i);
			mdelay(STEP_DELAY_MS);
		}
	}
}

/*
 * Task 2: Display Task (Independent Background Task)
 * Periodically prints "Hello from display task!" with a rotating spinner
 */
void task_display(void)
{
	static const char spinner[] = "-\\|/";
	uint32_t count = 0;

	while (1) {
		mdelay(2000);
		count++;

		/* Only stream to the console if running in FOREGROUND */
		if (task[currentTaskId].mode == MODE_FOREGROUND && usb_cdc_is_connected()) {
			console_puts("\r[DISPLAY] Hello from display task! [");
			console_putc(spinner[count % 4]);
			console_puts("] Tick: ");

			/* Format count number */
			char buf[12];
			int i = 0;
			uint32_t val = count;
			while (val > 0) {
				buf[i++] = '0' + (val % 10);
				val /= 10;
			}
			while (i > 0) {
				console_putc(buf[--i]);
			}
			console_puts("   ");
		}
	}
}

/*
 * This task will not have a while loop
 * It will simply exit after printing
 */
void task_hello(void)
{
	console_puts("Hello World!\n");
}

int main(void)
{
	/* 1. Hardware bootstrap (Clocks, GPIO, USB CDC) */
	bootstrap();

	/* 2. Initialize Hardware PWM Engine (2 kHz on GPIO 25 onboard LED) */
	pwm_init();

	arch_di();	// cli();

	/* 3. Initialize SysTick timer (1 ms periodic quantum) */
	timer_init();

	/* 4. Create primary interactive Shell Task (PID 0) */
	task_create_named("shell", shell_task, MODE_SYSTEM);

	/* 5. Start the round-robin multitasking engine */
	scheduler_start();

	/* Unreachable */
	while (1) {
		asm volatile("wfi");
	}

	return 0;
}
