ENTRY(_start)

SECTIONS
{
    . = 0x100000;

    .text : ALIGN(4k)
    {
        *(.text)
    }
    .rodata : ALIGN(4k)
    {
        *(.rodata)
    }
    .data : ALIGN(4k)
    {
        *(.data)
    }
    .bss : ALIGN(4k)
    {
        *(COMMON)
        *(.bss)
    }
}