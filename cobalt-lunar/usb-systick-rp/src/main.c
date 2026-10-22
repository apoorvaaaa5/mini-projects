#include <platform.h>
#include <usb_cdc.h>
#include <console.h>
#include <systick.h>
#include <arch.h>

static char prog[] = "-\\|/";
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
	counter = ms * 10; /* At 100 us/tick, 1 ms = 10 ticks */
	while (counter > 0)
	{
		usb_poll();
		asm volatile("wfi");
	}
}

int main(void)
{
	/* 1. Unified bootstrap sequence (Memory, Clocks, GPIO, USB) */
	bootstrap();

	/* 2. Enable Global CPU Interrupts */
	arch_ei();

	/* 3. Initialize SysTick with ISR callback */
	systick_init(isr_handler);
	systick_reload(100);

	uint32_t seconds = 0;
	int was_connected = 0;

	/* 4. Main loop driven by SysTick 1-second counter */
	while (1) {
		/* Toggle onboard green LED */
		plat_led_toggle();

		/* Update USB CDC Serial Terminal if connected */
		if (usb_cdc_is_connected()) {
			if (!was_connected) {
				was_connected = 1;
				console_puts("\r\n========================================\r\n");
				console_puts("RP2040 Bare-Metal USB CDC + SysTick\r\n");
				console_puts("========================================\r\n");
			}
			console_puts("\rRunning Blinky...[");
			console_putc(prog[seconds % 4]);
			console_puts("]");
		}
		seconds++;
		mdelay(1000);
	}

	return 0;
}
