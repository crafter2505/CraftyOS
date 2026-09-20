align 8

gdt_start:

    dd 0x00000000
    dd 0x00000000

    dw 0xFFFF
    dw 0x0000
    db 0x00
    db 10011010b
    db 11001111b        
    db 0x00

    dw 0x0000
    dw 0x0000
    db 0x00
    db 10011010b
    db 10101111b
    db 0x00

    dw 0xFFFF
    dw 0x0000
    db 0x00
    db 10010010b
    db 11001111b
    db 0x00

gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1
    dd gdt_start