#include <platform.h>

int main(void)
{
	plat_mem_init();
	plat_vreg_init();
	plat_clock_init();
	plat_gpio_init();

	while (1) {
		plat_led_toggle();
		for (volatile int i = 0; i < 10000000; i++);
	}

	return 0;
}
