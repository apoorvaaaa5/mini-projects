#include <stdio.h>
#include <timer.h>
#include <platform.h>
#include <arch.h>

int test_status;

int main()
{
	arch_di();
	plat_mem_init();
	plat_cpu_init();
	timer_one_shot(1000);
	puts("Hello world\n");
	for(int i = 0; i<10000; i++);
	if(test_status)
		puts("Timer test success!!\n");
	else	
		puts("Timer test failed!!\n");
	plat_stop_sim();
	
	return 0;
}
