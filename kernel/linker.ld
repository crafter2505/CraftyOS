ENTRY(_start)

KERNEL_VIRT_BASE = 0xFFFFFFFF80000000;
KERNEL_PHYS_BASE = 0x0000000000200000;

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

    .user_program ALIGN(4096) : AT(ADDR(.user_program) - KERNEL_VIRT_BASE + KERNEL_PHYS_BASE)
    {
        _user_program_start = .;
        *(.user_program)
        _user_program_end = .;
    }

    .user_program_file ALIGN(4096) : AT(ADDR(.user_program_file) - KERNEL_VIRT_BASE + KERNEL_PHYS_BASE)
    {
        _user_file_program_start = .;
        *(.user_program_file)
        _user_file_program_end = .;
    }

    .user_fork_program ALIGN(4096) : AT(ADDR(.user_fork_program) - KERNEL_VIRT_BASE + KERNEL_PHYS_BASE)
    {
        _user_fork_program_start = .;
        *(.user_fork_program)
        _user_fork_program_end = .;
    }

    .user_fork_exec ALIGN(4096) : AT(ADDR(.user_fork_exec) - KERNEL_VIRT_BASE + KERNEL_PHYS_BASE)
    {
        _user_fork_exec_start = .;
        *(.user_fork_exec)
        _user_fork_exec_end = .;
    }

    kernel_end = .;
}