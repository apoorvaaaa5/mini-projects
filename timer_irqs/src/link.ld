MEMORY
{
	mem : ORIGIN = 0X100000, LENGTH = 0X100000
}

SECTIONS
{
	.text : 
	{
		*(.vectors)
		*(.text.entry)
		*(.text.*)
	} > mem
	
	.rodata :
	{
		*(.rodata.*)
	} > mem

	.bss :
	{
		*(.bss)
		*(.bss.*)
		*(.sbss)
		*(.sbss.*)
	} > mem

	.data :
	{
		*(.data.*)
	} > mem

	.stack :
	{
		*(.stack)
		. = . + 4000;
		PROVIDE(_stack_start = .);
	} > mem

	PROVIDE(_bss_start = ADDR(.bss));
	PROVIDE(_bss_size = SIZEOF(.bss));
}

