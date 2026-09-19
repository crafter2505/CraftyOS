BITS 16
ORG 0x7E00

start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00
    sti

    mov si, msg_real
    call print_string_bios
    cli

    lgdt [gdt_descriptor]

    mov eax, cr0
    or eax, 1
    mov cr0, eax

    jmp 0x08:pm_entry

print_string_bios:
    mov ah, 0x0E
.next:
    lodsb
    cmp al, 0
    je .done
    int 0x10
    jmp .next
.done:
    ret

msg_real: db "Stage2: switching to protected mode...", 13, 10, 0

%include "bootloader/gdt.asm"

BITS 32
pm_entry:
    ; --- Your 32-bit code here ---
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov esp, 0x90000
    mov esi, msg_protected
    mov edi, 0xB8000
    mov ah, 0x0F

.write_loop:
    lodsb
    test al, al
    jz .done
    mov [edi], al
    mov [edi + 1], ah
    add edi, 2
    jmp .write_loop
.done:
    jmp .done

msg_protected: db "Hello from 32-bit protected mode!!!!!!! YEAAA", 0

times 1024 - ($ - $$) db 0