#include <platform.h>
#include <usb_cdc.h>
#include <console.h>
#include <systick.h>
#include <arch.h>
#include <pwm.h>

#define STEP_DELAY_MS  10  /* 10 ms per step -> 1.0 sec fade in, 1.0 sec fade out */

static volatile int counter = 0;

static void isr_handler(void)
{
	if (counter > 0) {
		counter--;
	}
	systick_reload(100); /* 100 us tick at 1 MHz */
}

void mdelay(uint32_t ms)
{
	counter = ms * 10; /* At 100 us per tick, 1 ms = 10 ticks */
	while (counter > 0)
	{
		usb_poll();
	}
}

int main(void)
{
	/* 1. Unified bootstrap sequence (Memory, Clocks, GPIO, USB) */
	bootstrap();

	/* 2. Enable Global CPU Interrupts */
	arch_ei();

	/* 3. Initialize SysTick with ISR callback for mdelay timebase */
	systick_init(isr_handler);
	systick_reload(100);

	/* 4. Initialize Hardware PWM Engine (2 kHz carrier frequency on GPIO 25) */
	pwm_init();

	int was_connected = 0;

	/* 5. Main Breathing Loop */
	while (1) {
		if (usb_cdc_is_connected()) {
			if (!was_connected) {
				was_connected = 1;
				console_puts("\r\n========================================\r\n");
				console_puts("RP2040 Hardware PWM Breathing LED (2 kHz)\r\n");
				console_puts("========================================\r\n");
			}
		}

		/* 1. Inhale: Ramp duty cycle up (0 -> 100%) */
		for (int i = 0; i <= 100; i++) {
			pwm_set(i);
			mdelay(STEP_DELAY_MS);
		}

		/* 2. Exhale: Ramp duty cycle down (100 -> 0%) */
		for (int i = 100; i >= 0; i--) {
			pwm_set(i);
			mdelay(STEP_DELAY_MS);
		}
	}

	return 0;
}
