#include <platform.h>
#include <usb_cdc.h>
#include <console.h>

static char prog[] = "-\\|/";
int main(void)
{
	/* 1. Run the unified bootstrap sequence (Memory, Clocks, GPIO, USB) */
	bootstrap();

	uint32_t poll_count = 0;
	uint32_t seconds = 0;
	int was_connected = 0;

	/* 2. Main USB Polling & Alive Heartbeat Loop */
	while (1) {
		/* Keep USB alive by servicing host transactions */
		usb_poll();

		/* Approximate 1-second interval counter at 125 MHz */
		poll_count++;
		if (poll_count >= 1500000) {
			poll_count = 0;
			seconds++;

			/* Toggle the onboard green LED so you visually see the chip is alive */
			plat_led_toggle();

			/* Check if a terminal is connected */
			if (usb_cdc_is_connected()) {
				if (!was_connected) {
					was_connected = 1;
					console_puts("\r\n========================================\r\n");
					console_puts("RP2040 Bare-Metal USB CDC Serial Console\r\n");
					console_puts("========================================\r\n");
				}
				console_puts("\rRunning Blinky...[");
				console_putc(prog[seconds % 4]);
				console_puts("]");
			}
		}
	}

	return 0;
}
