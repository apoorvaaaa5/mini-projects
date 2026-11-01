#ifndef SYSTICK_H
#define SYSTICK_H

#include <stdint.h>

void systick_init(void (*cb_func)(void));
void systick_reload(uint32_t ticks);
void systick_isr(void);

#endif /* SYSTICK_H */
