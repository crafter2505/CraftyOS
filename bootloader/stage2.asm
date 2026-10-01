BITS 16
ORG 0x7E00

KERNEL_ELF_BUFFER equ 0x400000

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
    call detect_memory

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

E820_COUNT_ADDR    equ 0x500
E820_CONTINUATION  equ 0x504
E820_ENTRIES       equ 0x508
E820_ENTRY_SIZE    equ 24
E820_MAX_ENTRIES   equ 32

detect_memory:
    push ax
    push bx
    push cx
    push dx
    push di
    push es
    push bp

    mov ax, 0
    mov es, ax
    mov di, E820_ENTRIES

    mov dword [E820_COUNT_ADDR], 0
    mov dword [E820_CONTINUATION], 0
    mov bp, E820_MAX_ENTRIES

.next_entry:
    test bp, bp
    jz .done
    dec bp

    mov eax, 0xE820
    mov edx, 0x534D4150
    mov ecx, E820_ENTRY_SIZE
    mov ebx, [E820_CONTINUATION]

    int 0x15

    jc .done
    cmp eax, 0x534D4150
    jne .done

    inc dword [E820_COUNT_ADDR]
    mov [E820_CONTINUATION], ebx

    add di, E820_ENTRY_SIZE

    test ebx, ebx
    jz .done
    jmp .next_entry

.done:
    pop bp
    pop es
    pop di
    pop dx
    pop cx
    pop bx
    pop ax
    ret

msg_real: db "Stage2: entering long mode...", 13, 10, 0

BITS 32
build_page_tables:
    mov edi, 0x10000
    mov dword [edi + 0*8], 0x11000 | 0x03
    mov dword [edi + 0*8 + 4], 0
    mov dword [edi + 256*8], 0x15000 | 0x03
    mov dword [edi + 256*8 + 4], 0
    mov dword [edi + 511*8], 0x13000 | 0x03
    mov dword [edi + 511*8 + 4], 0

    mov ecx, 0
.zero_pml4:
    cmp ecx, 512
    je .pml4_done
    cmp ecx, 0
    je .pml4_next
    cmp ecx, 256
    je .pml4_next
    cmp ecx, 511
    je .pml4_next
    mov dword [edi + ecx*8], 0
    mov dword [edi + ecx*8 + 4], 0
.pml4_next:
    inc ecx
    jmp .zero_pml4
.pml4_done:

    mov edi, 0x11000
    mov dword [edi + 0*8], 0x12000 | 0x03
    mov dword [edi + 0*8 + 4], 0
    mov ecx, 1
.zero_id_pdpt:
    cmp ecx, 512
    je .id_pdpt_done
    mov dword [edi + ecx*8], 0
    mov dword [edi + ecx*8 + 4], 0
    inc ecx
    jmp .zero_id_pdpt
.id_pdpt_done:

    mov edi, 0x12000
    mov ecx, 0
    mov eax, 0x83
.id_pd_loop:
    mov dword [edi + ecx*8], eax
    mov dword [edi + ecx*8 + 4], 0
    add eax, 0x200000
    inc ecx
    cmp ecx, 512
    jne .id_pd_loop

    mov edi, 0x13000
    mov ecx, 0
.zero_ker_pdpt:
    mov dword [edi + ecx*8], 0
    mov dword [edi + ecx*8 + 4], 0
    inc ecx
    cmp ecx, 512
    jne .zero_ker_pdpt
    mov dword [edi + 510*8], 0x14000 | 0x03
    mov dword [edi + 510*8 + 4], 0

    mov edi, 0x14000
    ; Map 4 entries x 2 MB = 8 MB at 0xFFFFFFFF80000000
    ; Physical base is 0x200000 (KERNEL_PHYS_BASE)
    mov ecx, 0
    mov eax, 0x200000 | 0x83
.kernel_pd_loop:
    mov dword [edi + ecx*8], eax
    mov dword [edi + ecx*8 + 4], 0
    add eax, 0x200000
    inc ecx
    cmp ecx, 4
    jne .kernel_pd_loop

.zero_kernel_pd:
    cmp ecx, 512
    je .kernel_pd_done
    mov dword [edi + ecx*8], 0
    mov dword [edi + ecx*8 + 4], 0
    inc ecx
    jmp .zero_kernel_pd
.kernel_pd_done:

    mov edi, 0x15000
    mov dword [edi + 0*8], 0x16000 | 0x03
    mov dword [edi + 0*8 + 4], 0
    mov ecx, 1
.zero_dm_pdpt:
    cmp ecx, 512
    je .dm_pdpt_done
    mov dword [edi + ecx*8], 0
    mov dword [edi + ecx*8 + 4], 0
    inc ecx
    jmp .zero_dm_pdpt
.dm_pdpt_done:

    mov edi, 0x16000
    mov ecx, 0
    mov eax, 0x83
.dm_pd_loop:
    mov dword [edi + ecx*8], eax
    mov dword [edi + ecx*8 + 4], 0
    add eax, 0x200000
    inc ecx
    cmp ecx, 512
    jne .dm_pd_loop

    ret

%include "bootloader/gdt.asm"


BITS 32
protected_mode_entry:
    mov ax, 0x18
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    mov esp, 0x90000

    call build_page_tables

    mov eax, 0x10000
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
    mov rdi, KERNEL_ELF_BUFFER
    mov esi, 40
    mov edx, 58
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
    mov r15, [KERNEL_ELF_BUFFER + 24]
    mov r13, [KERNEL_ELF_BUFFER + 32]
    movzx r14, word [KERNEL_ELF_BUFFER + 54]
    movzx r12, word [KERNEL_ELF_BUFFER + 56]

    lea rbx, [KERNEL_ELF_BUFFER + r13]

.segment_loop:
    test r12, r12
    jz .jump_kernel

    mov eax, [rbx]
    cmp eax, 1
    jne .next_segment

    mov r8, [rbx + 8]
    mov r9, [rbx + 16]
    mov rcx, [rbx + 32]

    lea rsi, [KERNEL_ELF_BUFFER + r8]
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

msg_long:   db "Hello from 64-bit long mode!", 0
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
