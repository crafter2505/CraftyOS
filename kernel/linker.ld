ENTRY(_start)

KERNEL_VIRT_BASE = 0xFFFFFFFF80000000;
KERNEL_PHYS_BASE = 0x0000000000100000;

SECTIONS
{
    . = KERNEL_VIRT_BASE;

    .text : AT(KERNEL_PHYS_BASE)
    {
        *(.text)
    }

    .rodata : AT(ADDR(.rodata) - KERNEL_VIRT_BASE + KERNEL_PHYS_BASE)
    {
        *(.rodata)
    }

    .data : AT(ADDR(.data) - KERNEL_VIRT_BASE + KERNEL_PHYS_BASE)
    {
        *(.data)
    }

    .bss : AT(ADDR(.bss) - KERNEL_VIRT_BASE + KERNEL_PHYS_BASE)
    {
        *(COMMON)
        *(.bss)
    }

    kernel_end = .;
}