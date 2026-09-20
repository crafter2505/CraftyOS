BITS 16
ORG 0x7E00

stage2_start:

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

    jmp 0x08:protected_mode_entry

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

msg_real: db "Stage2: entering long mode...", 13, 10, 0

%include "bootloader/gdt.asm"
%include "bootloader/paging.asm"

BITS 32
protected_mode_entry:
    ; Reload data segment registers with the data selector (0x18)
    mov ax, 0x18
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    ; Set up 32-bit stack
    mov esp, 0x90000

    mov eax, pml4_table
    mov cr3, eax

    mov eax, cr4
    or eax, (1 << 5)
    mov cr4, eax

    mov ecx, 0xC0000080
    rdmsr
    or eax, (1 << 8)
    wrmsr

    mov eax, cr0
    or eax, (1 << 31)
    mov cr0, eax

    
    jmp 0x10:long_mode_entry

BITS 64

long_mode_entry:
    mov ax, 0x18
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    mov rsp, 0x90000

    mov rsi, msg_long
    mov rdi, 0xB8000
    mov ah, 0x0F

.write_loop:
    lodsb
    test al, al
    jz .done
    mov [rdi], al
    mov [rdi + 1], ah
    add rdi, 2
    jmp .write_loop

.done:
    jmp .done

msg_long: db "Hello from 64-bit long mode! craftyOS rocks.", 0