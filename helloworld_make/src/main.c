#define mmio32(x)	(*(volatile unsigned int *) x)

static void putc(char c)
{
	mmio32(0x20000) = c;
}

static void puts(char* rs)
{
	while (*rs)
	{
		putc(*rs);
		rs++;
	}
}

static void stopsim()
{
	mmio32(0x20008) = 1;
}

int main()
{
	puts("Hello World\n");
	stopsim();
	return 0;
}
