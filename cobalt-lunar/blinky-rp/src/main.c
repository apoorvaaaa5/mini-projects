#include <platform.h>

int main()
{
	plat_mem_init();
	plat_gpio_init();

	while (1) {
		plat_led_toggle();
		for (volatile int i = 0; i < 500000; i++);
	}

	return 0;
}
