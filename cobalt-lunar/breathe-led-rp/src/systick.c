#include <stdint.h>
#include <mmio.h>
#include <systick.h>

#ifndef NULL
#define NULL ((void *)0)
#endif

/* ARM Cortex-M0+ SysTick Hardware Registers (Private Peripheral Bus) */
#define SYST_CSR            0xE000E010  /* Control and Status Register */
#define SYST_RVR            0xE000E014  /* Reload Value Register */
#define SYST_CVR            0xE000E018  /* Current Value Register */

/* SYST_CSR Bit Masks */
#define SYST_CSR_ENABLE     (1 << 0)    /* Counter Enable */
#define SYST_CSR_TICKINT    (1 << 1)    /* SysTick Exception Request Enable */
#define SYST_CSR_CLKSOURCE  (1 << 2)    /* 1 = CPU clk_sys (125 MHz) */

static void (*cb)(void) = NULL;

void systick_init(void (*cb_func)(void))
{
	cb = cb_func;

	/* 1. Disable SysTick during configuration */
	mmio32(SYST_CSR) = 0;

	/* 2. Set reload value for 1 ms tick:
	 *    clk_sys = 125 MHz -> 1 ms = 125,000 cycles.
	 *    Reload register takes N - 1 = 124,999.
	 */
	mmio32(SYST_RVR) = 0x00FFFFFFUL; // set max ticks

	/* 3. Clear current value and COUNTFLAG */
	mmio32(SYST_CVR) = 0;

	/* 4. Enable counter and select processor clock (IRQ disabled until reload) */
	mmio32(SYST_CSR) = SYST_CSR_ENABLE;
	//// Set this to 1mhz clock.
}

static void systick_enable_irq(void)
{
	mmio32(SYST_CSR) |= SYST_CSR_TICKINT;
}

static void systick_disable_irq(void)
{
	mmio32(SYST_CSR) &= ~SYST_CSR_TICKINT;
}

void systick_reload(uint32_t ticks)
{
	mmio32(SYST_RVR) = ticks;
	systick_enable_irq();
}

void systick_isr(void)
{
	systick_disable_irq();
	if(cb) cb();
}