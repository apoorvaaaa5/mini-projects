#include <platform.h>
#include <usb_cdc.h>
#include <console.h>
#include <pwm.h>
#include <arch.h>
#include <heap.h>
#include <systick.h>
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
 * In Foreground: Streams live ticks to the active cursor line.
 * In Background: Updates in-line relative telemetry above shell prompt.
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

		if (current_task && current_task->mode == MODE_FOREGROUND) {
			/* FOREGROUND: Stream directly to current terminal line */
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
		} else if (current_task && current_task->mode == MODE_BACKGROUND) {
			/* BACKGROUND: Update in-line relative telemetry above shell prompt */
			shell_update_bg_telemetry(display_spinner_char, count);
		}
	}
}

/*
 * Task 3: One-Shot Hello World Task
 * This task does NOT have an infinite while loop.
 * It cleanly prints in BOTH foreground and background, and auto-exits via task_exit_guard.
 */
void task_hello(void)
{
	if (!usb_cdc_is_connected()) return;

	if (current_task && current_task->mode == MODE_FOREGROUND) {
		/* Foreground: Standard output */
		console_puts("Hello World!\r\n");
	} else {
		/* Background: Event output with app prefix, and re-renders prompt! */
		console_puts("\r\n[hello] Hello World!\r\n");
		shell_prompt();
	}
}

int main(void)
{
	/* 1. Hardware bootstrap (Clocks, GPIO, USB CDC) */
	bootstrap();

	/* 2. Initialize 16 KB Dynamic Heap Allocator */
	heap_init();

	/* 3. Initialize Hardware PWM Engine (2 kHz on GPIO 25 onboard LED) */
	pwm_init();

	arch_di();	// cli();

	/* 4. Initialize SysTick timer (1 ms periodic quantum) */
	systick_init();

	/* 5. Dynamically allocate and spawn primary interactive Shell Task (PID 0) */
	task_create_named("shell", shell_task, MODE_SYSTEM);

	/* 6. Start the round-robin multitasking engine */
	scheduler_start();

	/* Unreachable */
	while (1) {
		arch_wfi();
	}

	return 0;
}
