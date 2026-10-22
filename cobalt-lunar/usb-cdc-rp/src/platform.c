#include <mmio.h>
#include <string.h>
#include <platform.h>
#include <usb_cdc.h>

/* ==========================================================================
 * Register Addresses & Offsets
 * ========================================================================== */

/* Resets Block */
#define RESETS_BASE             0x4000C000
#define RESETS_RESET            (RESETS_BASE + 0x00)
#define RESETS_RESET_DONE       (RESETS_BASE + 0x08)
#define RESETS_IO_BANK0_BIT     (1 << 5)
#define RESETS_PLL_SYS_BIT      (1 << 12)
#define RESETS_PLL_USB_BIT      (1 << 13)
#define RESETS_USBCTRL_BIT      (1 << 24)

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

/* System PLL (PLL_SYS at 0x40028000) */
#define PLL_SYS_BASE            0x40028000
#define PLL_SYS_CS              (PLL_SYS_BASE + 0x00)
#define PLL_SYS_PWR             (PLL_SYS_BASE + 0x04)
#define PLL_SYS_FBDIV_INT       (PLL_SYS_BASE + 0x08)
#define PLL_SYS_PRIM            (PLL_SYS_BASE + 0x0C)

/* USB PLL (PLL_USB at 0x4002C000) */
#define PLL_USB_BASE            0x4002C000
#define PLL_USB_CS              (PLL_USB_BASE + 0x00)
#define PLL_USB_PWR             (PLL_USB_BASE + 0x04)
#define PLL_USB_FBDIV_INT       (PLL_USB_BASE + 0x08)
#define PLL_USB_PRIM            (PLL_USB_BASE + 0x0C)

#define PLL_CS_LOCK_BIT         (1 << 31)

/* Clocks Block */
#define CLOCKS_BASE             0x40008000
#define CLK_REF_CTRL            (CLOCKS_BASE + 0x30)
#define CLK_REF_SELECTED        (CLOCKS_BASE + 0x38)
#define CLK_SYS_CTRL            (CLOCKS_BASE + 0x3C)
#define CLK_SYS_SELECTED        (CLOCKS_BASE + 0x44)
#define CLK_PERI_CTRL           (CLOCKS_BASE + 0x48)
#define CLK_USB_CTRL            (CLOCKS_BASE + 0x54)
#define CLK_USB_DIV             (CLOCKS_BASE + 0x58)

/* GPIO & SIO */
#define IO_BANK0_GPIO25_CTRL    0x400140CC
#define SIO_GPIO_OUT_SET        0xD0000014
#define SIO_GPIO_OUT_CLR        0xD0000018
#define SIO_GPIO_OUT_XOR        0xD000001C
#define SIO_GPIO_OE_SET         0xD0000024
#define PICO_LED_PIN            25

/* Linker symbols */
extern void *_data_start, *_data_vstart, *_data_size;
extern void *_bss_start, *_bss_size;

static void plat_mem_init(void)
{
	memcpy(&_data_vstart, &_data_start, (unsigned int)&_data_size);
	memset(&_bss_start, 0, (unsigned int)&_bss_size);
}

static void plat_vreg_init(void)
{
	/* Configure VREG to 1.10V nominal */
	mmio32(VREG_CTRL) = VREG_VSEL_1_10V | (1 << 0);
	while ((mmio32(VREG_CTRL) & VREG_ROK_BIT) == 0);
}

static void plat_clocks_init(void)
{
	/* 1. Start 12 MHz External Crystal (XOSC) */
	mmio32(XOSC_STARTUP) = 47;
	mmio32(XOSC_CTRL) = XOSC_CTRL_ENABLE | XOSC_CTRL_1_15MHZ;
	while ((mmio32(XOSC_STATUS) & XOSC_STATUS_STABLE_BIT) == 0);

	/* 2. Switch CLK_REF to XOSC */
	mmio32(CLK_REF_CTRL) = 0x2;
	while ((mmio32(CLK_REF_SELECTED) & (1 << 2)) == 0);

	/* 3. Temporarily switch CLK_SYS to CLK_REF before tuning PLL_SYS */
	mmio32(CLK_SYS_CTRL) = 0x0;
	while ((mmio32(CLK_SYS_SELECTED) & (1 << 0)) == 0);

	/* 4. Setup PLL_SYS for 125 MHz (Fvco = 1500 MHz, PostDiv = 6 * 2) */
	mmio32(RESETS_RESET) &= ~RESETS_PLL_SYS_BIT;
	while ((mmio32(RESETS_RESET_DONE) & RESETS_PLL_SYS_BIT) == 0);

	mmio32(PLL_SYS_CS) = 1;          /* REFDIV = 1 */
	mmio32(PLL_SYS_FBDIV_INT) = 125; /* FBDIV = 125 */
	mmio32(PLL_SYS_PWR) &= ~((1 << 0) | (1 << 5)); /* Power on VCO */
	while ((mmio32(PLL_SYS_CS) & PLL_CS_LOCK_BIT) == 0);

	mmio32(PLL_SYS_PRIM) = (6 << 16) | (2 << 12);
	mmio32(PLL_SYS_PWR) &= ~(1 << 3); /* Power on post-dividers */

	/* Switch CLK_SYS to PLL_SYS */
	mmio32(CLK_SYS_CTRL) = (0 << 5) | (0 << 0);
	mmio32(CLK_SYS_CTRL) = (0 << 5) | (1 << 0);
	while ((mmio32(CLK_SYS_SELECTED) & (1 << 1)) == 0);

	/* Enable Peripheral Clock (125 MHz) */
	mmio32(CLK_PERI_CTRL) = (1 << 11) | (0 << 5);

	/* 5. Setup PLL_USB for 48.0 MHz:
	 *    Fin = 12 MHz, RefDiv = 1
	 *    FbDiv = 80 -> Fvco = 12 * 80 = 960 MHz
	 *    PostDiv1 = 5, PostDiv2 = 4 -> Fout = 960 / (5 * 4) = 48.0 MHz
	 */
	mmio32(RESETS_RESET) &= ~RESETS_PLL_USB_BIT;
	while ((mmio32(RESETS_RESET_DONE) & RESETS_PLL_USB_BIT) == 0);

	mmio32(PLL_USB_CS) = 1;         /* REFDIV = 1 */
	mmio32(PLL_USB_FBDIV_INT) = 80; /* FBDIV = 80 */
	mmio32(PLL_USB_PWR) &= ~((1 << 0) | (1 << 5)); /* Power on VCO */
	while ((mmio32(PLL_USB_CS) & PLL_CS_LOCK_BIT) == 0);

	mmio32(PLL_USB_PRIM) = (5 << 16) | (4 << 12);
	mmio32(PLL_USB_PWR) &= ~(1 << 3); /* Power on post-dividers */

	/* Route PLL_USB (AUXSRC = 0, clksrc_pll_usb = 48.0 MHz) to CLK_USB and enable */
	mmio32(CLK_USB_DIV) = (1 << 8);              /* Div = 1.0 */
	mmio32(CLK_USB_CTRL) = (1 << 11) | (0 << 5); /* ENABLE (bit 11) | AUXSRC = 0 (clksrc_pll_usb) */
}

static void plat_gpio_init(void)
{
	/* De-assert reset on IO_BANK0 */
	mmio32(RESETS_RESET) &= ~RESETS_IO_BANK0_BIT;
	while ((mmio32(RESETS_RESET_DONE) & RESETS_IO_BANK0_BIT) == 0);

	/* Set GPIO 25 function to SIO (5) */
	mmio32(IO_BANK0_GPIO25_CTRL) = 5;

	/* Enable GPIO 25 output in SIO */
	mmio32(SIO_GPIO_OE_SET) = (1 << PICO_LED_PIN);
}

void plat_led_set(int on)
{
	if (on) {
		mmio32(SIO_GPIO_OUT_SET) = (1 << PICO_LED_PIN);
	} else {
		mmio32(SIO_GPIO_OUT_CLR) = (1 << PICO_LED_PIN);
	}
}

void plat_led_toggle(void)
{
	mmio32(SIO_GPIO_OUT_XOR) = (1 << PICO_LED_PIN);
}

/* ==========================================================================
 * The Unified Bootstrap Function
 * ========================================================================== */
void bootstrap(void)
{
	plat_mem_init();
	plat_vreg_init();
	plat_clocks_init();
	plat_gpio_init();
	usb_init();
}
