#include <task.h>
#include <platform.h>
#include <usb_cdc.h>
#include <console.h>
#include <pwm.h>
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
 * Task 2: Display Task (Continuous Background / Foreground Task)
 * In Foreground: Streams live ticks to the active cursor line in-place.
 */
static volatile uint32_t display_tick_count = 0;
static volatile char display_spinner_char = '-';

uint32_t display_get_tick(void) { return display_tick_count; }
char display_get_spinner(void) { return display_spinner_char; }

void task_display(void)
{
	static const char spinner[] = "/-\\|";
	uint32_t count = 0;

	while (1) {
		mdelay(2000);
		count++;
		display_tick_count = count;
		display_spinner_char = spinner[count % 4];

		if (!usb_cdc_is_connected()) continue;

		if (shell_is_foreground()) {
			/* FOREGROUND: Stream directly to current terminal line in-place */
			console_puts("\r[DISPLAY] Hello from display task! [");
			console_putc(display_spinner_char);
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
		} else {
			/* BACKGROUND: Update in-line relative telemetry above shell prompt */
			shell_update_bg_telemetry(display_spinner_char, count);
		}
	}
}

/*
 * Task 3: One-Shot Hello World Task
 * Auto-terminates and cleans up via task_exit_guard upon return.
 */
void task_hello(void)
{
	if (!usb_cdc_is_connected()) return;

	if (shell_is_foreground()) {
		/* Foreground: Standard output */
		console_puts("Hello World!\r\n");
	} else {
		/* Background: Event output with app prefix, and re-renders prompt! */
		console_puts("\r\n[hello] Hello World!\r\n");
		shell_prompt();
	}
}

/*
 * Task 4: EEMBC CoreMark 1.0 Benchmark Task
 */
extern int coremark_main(void);

void task_coremark(void)
{
	console_puts("\r\n=======================================================\r\n");
	console_puts("   EEMBC CoreMark 1.0 Benchmark (StrataHalo OS)        \r\n");
	console_puts("   Target: RP2040 Cortex-M0+ @ 125 MHz                 \r\n");
	console_puts("   Workload: 2,000 Iterations (Floating Point Enabled) \r\n");
	console_puts("   Allocating 2000-byte data block from Dynamic Heap   \r\n");
	console_puts("=======================================================\r\n\r\n");

	coremark_main();

	console_puts("\r\n[OK] CoreMark Benchmark run completed successfully.\r\n");
}
