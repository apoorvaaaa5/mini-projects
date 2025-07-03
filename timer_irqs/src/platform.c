#include <mmio.h>
#include <string.h>
#include <arch.h>

unsigned long long plat_clint_timer_read()
{	
	unsigned int tl, th, temp;
	do
	{
		temp = mmio32(0x30004);
		tl = mmio32(0x30000);
		th = mmio32(0x30004);
	} while(th != temp);
	
	return ((unsigned long long) th << 32) |
		(unsigned long long) tl;
}

void plat_clint_timer_compare(unsigned long long cmp)
{
	unsigned int cmp_l, cmp_h;
	cmp_l = (unsigned int)(cmp & ((1ULL<<32)-1));
	cmp_h = (unsigned int)(cmp >> 32);
	mmio32(0x30008) = cmp_l;
	mmio32(0x3000C) = cmp_h;
}
	
void plat_ascii_out(char c)
{
	mmio32(0x20000) = c;
	asm volatile ("fence rw,rw");
}

void plat_stop_sim()
{
	mmio32(0x20008) = 1;
}

void plat_mem_init()
{
	extern void *_bss_start, *_bss_size;
	memset(&_bss_start, 0, (unsigned int) &_bss_size);
}

void plat_cpu_init()
{
	arch_ei();
}
