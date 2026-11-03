#include <console.h>
#include <usb_cdc.h>
#include <string.h>
#include <scheduler.h>

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
	if (usb_cdc_read(&ch, 1) != 1) {
		return -1;
	}

	/*
	 * Escape sequence interceptor:
	 * Swallows ALL arrow keys (\033[A, \033[B), mouse scroll, and escape sequences
	 * so they NEVER leak into the shell buffer as '[A[A[A[B'!
	 */
	if (ch == 0x1B) {
		int count = 0;
		uint32_t start_poll = system_millis;
		while ((system_millis - start_poll) < 25 && count < 28) {
			usb_poll();
			uint8_t next_ch;
			if (usb_cdc_read(&next_ch, 1) == 1) {
				count++;
				if (next_ch == 'R' || (next_ch >= 'A' && next_ch <= 'Z') || next_ch == '~') {
					break;
				}
				start_poll = system_millis;
			}
		}

		/* Standalone ESC key */
		if (count == 0) {
			return 0x1B;
		}

		/* Swallow all arrow keys, mouse wheel codes, etc. */
		return -1;
	}

	return (int)ch;
}

/* Simple atomic lock for serial TX to prevent interleaving */
static volatile int tx_lock = 0;

void console_lock(void)
{
	tx_lock = 1;
}

void console_unlock(void)
{
	tx_lock = 0;
}

int console_is_locked(void)
{
	return tx_lock;
}

void console_print_dec(uint32_t val)
{
	char buf[12];
	int i = 0;
	if (val == 0) {
		console_putc('0');
		return;
	}
	while (val > 0) {
		buf[i++] = '0' + (val % 10);
		val /= 10;
	}
	while (i > 0) {
		console_putc(buf[--i]);
	}
}

void console_print_hex(uint32_t val)
{
	static const char hex[] = "0123456789ABCDEF";
	console_puts("0x");
	for (int i = 7; i >= 0; i--) {
		console_putc(hex[(val >> (i * 4)) & 0xF]);
	}
}

void console_print_byte_hex(uint8_t b)
{
	static const char hex[] = "0123456789ABCDEF";
	console_putc(hex[(b >> 4) & 0xF]);
	console_putc(hex[b & 0xF]);
}


