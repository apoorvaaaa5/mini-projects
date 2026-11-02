#include <systick.h>
#include <mmio.h>

/* ARM Cortex-M0+ SysTick Hardware Registers (Private Peripheral Bus) */
#define SYST_CSR            0xE000E010  /* Control and Status Register */
#define SYST_RVR            0xE000E014  /* Reload Value Register */
#define SYST_CVR            0xE000E018  /* Current Value Register */

/* SYST_CSR Bit Masks */
#define SYST_CSR_ENABLE     (1 << 0)    /* Counter Enable */
#define SYST_CSR_TICKINT    (1 << 1)    /* SysTick Exception Request Enable */
#define SYST_CSR_CLKSOURCE  (1 << 2)    /* 1 = CPU clk_sys (125 MHz), 0 = 1 MHz ref */

void systick_init(void)
{
	/* 1. Disable SysTick during configuration */
	mmio32(SYST_CSR) = 0;

	/* 2. Set reload value for 1 ms tick using 1 MHz reference clock:
	 *    1 MHz clock -> 1,000 cycles per ms.
	 *    Reload register takes N - 1 = 999.
	 */
	mmio32(SYST_RVR) = 999;

	/* 3. Clear current counter */
	mmio32(SYST_CVR) = 0;

	/* 4. Enable counter and SysTick interrupt using 1 MHz reference clock (CLKSOURCE = 0) */
	mmio32(SYST_CSR) = SYST_CSR_ENABLE | SYST_CSR_TICKINT;
}
