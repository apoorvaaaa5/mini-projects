#ifndef CONSOLE_H
#define CONSOLE_H

#include <stdint.h>

void console_putc(char c);
void console_puts(const char *str);
void console_write(const char *buf, uint32_t len);
int  console_getc(void);

/* Numeric and Formatted Printing Helpers */
void console_print_dec(uint32_t val);
void console_print_hex(uint32_t val);
void console_print_byte_hex(uint8_t b);

/* Console TX Mutex for Concurrency Safety */
void console_lock(void);
void console_unlock(void);
int  console_is_locked(void);

#endif /* CONSOLE_H */
