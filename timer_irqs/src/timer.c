#include <platform.h>
#include <arch.h>
#include <stdio.h>

void timer_one_shot(unsigned int ticks)
{
	unsigned long long timer;
	timer = plat_clint_timer_read();
	timer += (unsigned long long) ticks;
	plat_clint_timer_compare(timer); 
	arch_ei_mtime();
}

void timer_isr()
{
	extern int test_status;
	arch_di_mtime();
	test_status = 1;
	puts("\nThis is timer interrupttttt\n");
}
