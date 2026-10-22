#include <mmio.h>
#include <string.h>
#include <platform.h>

/* ==========================================================================
 * Register Base Addresses & Offsets
 * ========================================================================== */

/* Resets Block */
#define RESETS_BASE             0x4000C000
#define RESETS_RESET            (RESETS_BASE + 0x00)
#define RESETS_RESET_DONE       (RESETS_BASE + 0x08)
#define RESETS_IO_BANK0_BIT     (1 << 5)
#define RESETS_PLL_SYS_BIT      (1 << 12)

/* Voltage Regulator (VREG) */
#define VREG_BASE               0x40064000
#define VREG_CTRL               (VREG_BASE + 0x00)
#define VREG_ROK_BIT            (1 << 12)
#define VREG_VSEL_1_10V         (0xB << 4)

/* Crystal Oscillator (XOSC) */
#define XOSC_BASE               0x40024000
#define XOSC_CTRL               (XOSC_BASE + 0x00)
#define XOSC_STATUS             (XOSC_BASE + 0x04)
#define XOSC_STARTUP            (XOSC_BASE + 0x0C)
#define XOSC_CTRL_ENABLE        (0xFAB << 12)
#define XOSC_CTRL_1_15MHZ       (0xAA0)
#define XOSC_STATUS_STABLE_BIT  (1 << 31)

/* System PLL (PLL_SYS) */
#define PLL_SYS_BASE            0x40028000
#define PLL_SYS_CS              (PLL_SYS_BASE + 0x00)
#define PLL_SYS_PWR             (PLL_SYS_BASE + 0x04)
#define PLL_SYS_FBDIV_INT       (PLL_SYS_BASE + 0x08)
#define PLL_SYS_PRIM            (PLL_SYS_BASE + 0x0C)
#define PLL_SYS_CS_LOCK_BIT     (1 << 31)

/* Clocks Block */
#define CLOCKS_BASE             0x40008000
#define CLK_REF_CTRL            (CLOCKS_BASE + 0x30)
#define CLK_REF_SELECTED        (CLOCKS_BASE + 0x38)
#define CLK_SYS_CTRL            (CLOCKS_BASE + 0x3C)
#define CLK_SYS_SELECTED        (CLOCKS_BASE + 0x44)
#define CLK_PERI_CTRL           (CLOCKS_BASE + 0x48)

/* GPIO & SIO */
#define IO_BANK0_GPIO25_CTRL    0x400140CC
#define SIO_GPIO_OUT_XOR        0xD000001C
#define SIO_GPIO_OE_SET         0xD0000024
#define PICO_LED_PIN            25

/* Linker symbols */
extern void *_data_start, *_data_vstart, *_data_size;
extern void *_bss_start, *_bss_size;

void plat_mem_init(void)
{
	memcpy(&_data_vstart, &_data_start, (unsigned int)&_data_size);
	memset(&_bss_start, 0, (unsigned int)&_bss_size);
}

void plat_vreg_init(void)
{
	/* Configure VREG to 1.10V (nominal) and keep enabled */
	mmio32(VREG_CTRL) = VREG_VSEL_1_10V | (1 << 0);

	/* Wait until output voltage is in regulation (ROK bit 12) */
	while ((mmio32(VREG_CTRL) & VREG_ROK_BIT) == 0);
}

void plat_clock_init(void)
{
	/* 1. Start the 12.0 MHz Crystal Oscillator (XOSC) */
	mmio32(XOSC_STARTUP) = 47;  /* ~1ms startup delay counter */
	mmio32(XOSC_CTRL) = XOSC_CTRL_ENABLE | XOSC_CTRL_1_15MHZ;
	while ((mmio32(XOSC_STATUS) & XOSC_STATUS_STABLE_BIT) == 0);

	/* 2. Switch CLK_REF to XOSC (source 0x2 = XOSC) */
	mmio32(CLK_REF_CTRL) = 0x2;
	while ((mmio32(CLK_REF_SELECTED) & (1 << 2)) == 0);

	/* 3. Temporarily switch CLK_SYS to CLK_REF before touching PLL_SYS */
	mmio32(CLK_SYS_CTRL) = 0x0; /* SRC = 0 (clk_ref) */
	while ((mmio32(CLK_SYS_SELECTED) & (1 << 0)) == 0);

	/* 4. Release PLL_SYS from reset */
	mmio32(RESETS_RESET) &= ~RESETS_PLL_SYS_BIT;
	while ((mmio32(RESETS_RESET_DONE) & RESETS_PLL_SYS_BIT) == 0);

	/* 5. Configure PLL_SYS for 125 MHz:
	 *    Fin = 12 MHz, RefDiv = 1 -> Fref = 12 MHz
	 *    FbDiv = 125 -> Fvco = 12 * 125 = 1500 MHz
	 *    PostDiv1 = 6, PostDiv2 = 2 -> Fout = 1500 / (6 * 2) = 125 MHz
	 */
	mmio32(PLL_SYS_CS) = 1;                     /* REFDIV = 1 */
	mmio32(PLL_SYS_FBDIV_INT) = 125;            /* FBDIV_INT = 125 */

	/* Power up VCO and PLL (clear PD bit 0 and VCOPD bit 5) */
	mmio32(PLL_SYS_PWR) &= ~((1 << 0) | (1 << 5));

	/* Wait for PLL to lock */
	while ((mmio32(PLL_SYS_CS) & PLL_SYS_CS_LOCK_BIT) == 0);

	/* Configure post-dividers and power them on (clear POSTDIVPD bit 3) */
	mmio32(PLL_SYS_PRIM) = (6 << 16) | (2 << 12);
	mmio32(PLL_SYS_PWR) &= ~(1 << 3);

	/* 6. Switch CLK_SYS to PLL_SYS:
	 *    Set auxiliary mux AUXSRC to 0 (clksrc_pll_sys),
	 *    then switch glitchless main mux SRC to 1 (clksrc_clk_sys_aux).
	 */
	mmio32(CLK_SYS_CTRL) = (0 << 5) | (0 << 0);
	mmio32(CLK_SYS_CTRL) = (0 << 5) | (1 << 0);
	while ((mmio32(CLK_SYS_SELECTED) & (1 << 1)) == 0);

	/* 7. Enable CLK_PERI (Peripheral Clock) sourced from CLK_SYS (125 MHz) */
	mmio32(CLK_PERI_CTRL) = (1 << 11) | (0 << 5);
}

void plat_gpio_init(void)
{
	/* De-assert reset on IO_BANK0 (bit 5) and wait */
	mmio32(RESETS_RESET) &= ~RESETS_IO_BANK0_BIT;
	while ((mmio32(RESETS_RESET_DONE) & RESETS_IO_BANK0_BIT) == 0);

	/* Set GPIO 25 function to SIO (5) */
	mmio32(IO_BANK0_GPIO25_CTRL) = 5;

	/* Enable GPIO 25 output in SIO */
	mmio32(SIO_GPIO_OE_SET) = (1 << PICO_LED_PIN);
}

void plat_led_toggle(void)
{
	mmio32(SIO_GPIO_OUT_XOR) = (1 << PICO_LED_PIN);
}
