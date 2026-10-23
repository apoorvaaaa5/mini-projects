#ifndef _ARCH_H_
#define _ARCH_H_

#include <stdint.h>

/* =========================================================================
 * 1. Global CPU Interrupt Control (ARM Cortex-M0+ PRIMASK register)
 * ========================================================================= */

/* Enable all interrupts on ARM Cortex-M0+ */
static inline void arch_ei(void)
{
	asm volatile("cpsie i" : : : "memory");
}

/* Disable all interrupts on ARM Cortex-M0+ */
static inline void arch_di(void)
{
	asm volatile("cpsid i" : : : "memory");
}

/* Stall CPU until next interrupt (Wait For Interrupt) */
static inline void arch_wfi(void)
{
	asm volatile("wfi");
}

#endif /* _ARCH_H_ */
