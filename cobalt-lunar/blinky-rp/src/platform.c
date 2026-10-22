#include <mmio.h>
#include <string.h>
#include <platform.h>

#define RESETS_RESET            0x4000C000
#define RESETS_RESET_DONE       0x4000C008

#define IO_BANK0_GPIO25_CTRL    0x400140CC

#define SIO_GPIO_OUT_XOR        0xD000001C
#define SIO_GPIO_OE_SET         0xD0000024

#define PICO_LED_PIN            25

extern void *_data_start, *_data_vstart, *_data_size;
extern void *_bss_start, *_bss_size;

void plat_mem_init()
{
	memcpy(&_data_vstart, &_data_start, (unsigned int)&_data_size);
	memset(&_bss_start, 0, (unsigned int)&_bss_size);
}

void plat_gpio_init()
{
	/* De-assert reset on IO_BANK0 (bit 5) and wait */
	mmio32(RESETS_RESET) &= ~(1 << 5);
	while ((mmio32(RESETS_RESET_DONE) & (1 << 5)) == 0);

	/* Set GPIO 25 function to SIO (5) */
	mmio32(IO_BANK0_GPIO25_CTRL) = 5;

	/* Enable GPIO 25 output in SIO */
	mmio32(SIO_GPIO_OE_SET) = (1 << PICO_LED_PIN);
}

void plat_led_toggle()
{
	mmio32(SIO_GPIO_OUT_XOR) = (1 << PICO_LED_PIN);
}
