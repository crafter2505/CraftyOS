BITS 16
ORG 0x7C00

STAGE2_ADDR equ 0x7E00
STAGE2_SECTORS equ 80

start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00
    mov [boot_drive], dl

    mov si, msg_loading
    call print_string

    mov ah, 0x02
    mov al, STAGE2_SECTORS
    mov ch, 0
    mov cl, 2
    mov dh, 0
    mov dl, [boot_drive]
    mov bx, STAGE2_ADDR
    int 0x13
    jc disk_error

    jmp STAGE2_ADDR
disk_error:
    mov si, msg_error
    call print_string
    jmp hang

hang:
    jmp hang

print_string:
    mov ah, 0x0E

.next:
    lodsb
    cmp al, 0
    je .done
    int 0x10
    jmp .next
.done:
    ret

boot_drive: db 0
msg_loading: db "Stage 1 loading stage 2...", 13, 10, 0    
msg_error: db "Stage1: disk read failed", 13, 10, 0

times 510 - ($ - $$) db 0
dw 0xAA55
