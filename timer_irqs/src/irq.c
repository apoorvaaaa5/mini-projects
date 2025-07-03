#include <timer.h>
#include <platform.h>
#include <stdio.h>

void isr_hndlr(unsigned int id)
{
	if (id == (1<<31|7))
	{
		timer_isr();
	}
	else
	{
		puts("Illegal Exception : Stopping\n");
		plat_stop_sim();
	}
	
}

