#include <console.h>
#include <usb_cdc.h>
#include <string.h>

void console_write(const char *buf, uint32_t len)
{
	/* Do not block or loop forever if USB is not configured */
	if (!usb_cdc_is_connected()) {
		return;
	}

	uint32_t sent = 0;
	uint32_t retries = 0;
	while (sent < len && retries < 10000) {
		uint32_t chunk = len - sent;
		if (chunk > 64) chunk = 64;

		int written = usb_cdc_write((const uint8_t *)(buf + sent), chunk);
		if (written > 0) {
			sent += written;
			retries = 0;
		} else {
			retries++;
		}
		usb_poll();
	}
}

void console_putc(char c)
{
	if (c == '\n') {
		char r = '\r';
		console_write(&r, 1);
	}
	console_write(&c, 1);
}

void console_puts(const char *str)
{
	if (!str) return;
	console_write(str, strlen(str));
}

int console_getc(void)
{
	uint8_t ch = 0;
	usb_poll();
	if (usb_cdc_read(&ch, 1) == 1) {
		return (int)ch;
	}
	return -1; /* No character available */
}

int console_has_char(void)
{
	usb_poll();
	return 0; /* Handled directly by console_getc returning -1 when empty */
}
