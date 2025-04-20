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

	.stack :
	{
		*(.stack)
		. = . + 100;
		PROVIDE(_stack_start = .);
	} > mem
}

