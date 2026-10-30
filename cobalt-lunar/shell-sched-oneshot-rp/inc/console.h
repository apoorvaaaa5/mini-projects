#ifndef CONSOLE_H
#define CONSOLE_H

#include <stdint.h>

void console_putc(char c);
void console_puts(const char *str);
void console_write(const char *buf, uint32_t len);
int  console_getc(void);
int  console_has_char(void);

#endif /* CONSOLE_H */
