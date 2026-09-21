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

.print1:
    lodsb
    test al, al
    jz .load_kernel
    mov [rdi], al
    mov [rdi + 1], ah
    add rdi, 2
    jmp .print1

.load_kernel:
    mov rdi, 0x200000
    mov esi, 40
    mov edx, 40
    call ata_read_sectors

    mov rsi, msg_kernel
    mov rdi, 0xB8000 + 160
    mov ah, 0x0F

.print2:
    lodsb
    test al, al
    jz .parse_elf
    mov [rdi], al
    mov [rdi + 1], ah
    add rdi, 2
    jmp .print2

.parse_elf:
    mov r15, [0x200000 + 24]
    mov r13, [0x200000 + 32]
    movzx r14, word [0x200000 + 54]
    movzx r12, word [0x200000 + 56]

    lea rbx, [0x200000 + r13]
.segment_loop:
    test r12, r12
    jz .jump_kernel
    mov eax, [rbx]
    cmp eax, 1
    jne .next_segment

    mov r8, [rbx + 8]
    mov r9, [rbx + 16]
    mov rcx, [rbx + 32]

    lea rsi, [0x200000 + r8]

    mov rdi, r9
    cld
    rep movsb

.next_segment:
    add rbx, r14
    dec r12
    jmp .segment_loop
.jump_kernel:
    jmp r15
.hang:
    jmp .hang

msg_long: db "Hello from 64-bit long mode!", 0
msg_kernel: db "Kernel loaded, parsing ELF...", 0

ata_read_sectors:
    mov r10d, edx


.next_sector:

.wait_not_busy:
    mov dx, 0x1F7
    in al, dx
    test al, 0x80
    jnz .wait_not_busy

    mov eax, esi
    shr eax, 24
    or al, 0xE0
    mov dx, 0x1F6
    out dx, al

    mov dx, 0x1F2
    mov al, 1
    out dx, al

    mov eax, esi
    mov dx, 0x1F3
    out dx, al

    mov eax, esi
    shr eax, 8
    mov dx, 0x1F4
    out dx, al

    mov eax, esi
    shr eax, 16
    mov dx, 0x1F5
    out dx, al

    mov dx, 0x1F7
    mov al, 0x20
    out dx, al



.wait_data:
    mov dx, 0x1F7
    in al, dx
    test al, 0x08
    jz .wait_data

    mov dx, 0x1F0
    mov rcx, 128

.read_dword:
    in eax, dx
    mov [rdi], eax
    add rdi, 4
    loop .read_dword

    inc esi
    dec r10d
    jnz .next_sector

    ret