#include <platform.h>

void putc(char c)
{
	plat_ascii_out(c);
}

void puts(char* rs)
{
	while (*rs)
	{
		putc(*rs);
		rs++;
	}
}

