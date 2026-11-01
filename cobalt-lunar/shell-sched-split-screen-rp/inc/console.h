#ifndef CONSOLE_H
#define CONSOLE_H

#include <stdint.h>

void console_putc(char c);
void console_puts(const char *str);
void console_write(const char *buf, uint32_t len);
int  console_getc(void);
int  console_has_char(void);

/* Split-Screen 65:35 Dynamic TUI Engine APIs */
void console_split_activate(void);
void console_split_deactivate(void);
int  console_split_is_active(void);
void console_split_resize(void);
void console_split_clear_left(void);
void console_split_render_telemetry(void);

/* Console TX Mutex for Concurrency Safety */
void console_lock(void);
void console_unlock(void);
int  console_is_locked(void);

#endif /* CONSOLE_H */
