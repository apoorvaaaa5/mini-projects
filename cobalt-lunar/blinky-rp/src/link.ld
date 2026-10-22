MEMORY
{
    flash (rx)  : ORIGIN = 0x10000000, LENGTH = 2048K
    sram  (rwx) : ORIGIN = 0x20000000, LENGTH = 256K
}

ENTRY(entry)

SECTIONS
{
    /* RP2040 Boot ROM Stage 2 Bootloader (Exactly 256 bytes at start of flash) */
    .boot2 :
    {
        . = ALIGN(4);
        KEEP(*(.boot2))
    } > flash

    . = ORIGIN(flash) + 0x100;

    .vectors :
    {
        . = ALIGN(4);
        KEEP(*(.vectors))
    } > flash

    .text :
    {
        . = ALIGN(4);
        *(.text.entry)
        *(.text)
        *(.text.*)
    } > flash

    .rodata :
    {
        . = ALIGN(4);
        *(.rodata)
        *(.rodata.*)
    } > flash

    .data :
    {
        . = ALIGN(4);
        _data_vstart = .;
        *(.data)
        *(.data.*)
        . = ALIGN(4);
        _data_vend = .;
    } > sram AT > flash

    _data_start = LOADADDR(.data);
    _data_size  = SIZEOF(.data);

    .bss :
    {
        . = ALIGN(4);
        _bss_start = .;
        *(.bss)
        *(.bss.*)
        *(COMMON)
        . = ALIGN(4);
        _bss_end = .;
    } > sram

    _bss_size = SIZEOF(.bss);

    .stack (NOLOAD) :
    {
        . = ALIGN(8);
        . = . + 4096;
        PROVIDE(_stack_start = .);
    } > sram
}
