BITS 16
ORG 0x7E00

start:
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7c00

    mov si, msg
    call print_string

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

msg: db 13, 10, "Stage2: CraftyOS loaded!!!", 13, 10, "Now The Bootloader does the real work.", 13, 10 ,0

times 512 - ($ - $$) db 0