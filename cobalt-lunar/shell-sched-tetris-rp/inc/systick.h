#ifndef SYSTICK_H
#define SYSTICK_H

#include <stdint.h>

/* Initialize ARM Cortex-M0+ SysTick timer (1 ms periodic quantum using 1 MHz ref clock) */
void systick_init(void);

#endif /* SYSTICK_H */
