# Makefile - craftyOS build system

ASM      := nasm
ASMFLAGS := -f bin
CC       := gcc
CFLAGS   := -m64 -ffreestanding -fno-pic -fno-stack-protector -nostdlib -Wall -Wextra -c
LD       := ld
LDFLAGS  := -m elf_x86_64 -T kernel/linker.ld -nostdlib -static

IMG        := CraftyOS.img
STAGE1     := bootloader/stage1.asm
STAGE2     := bootloader/stage2.asm

KERNEL_SRCS := kernel/kernel.c kernel/vga.c kernel/terminal.c kernel/kprintf.c kernel/pic.c kernel/idt.c kernel/isr.c kernel/timer.c kernel/keyboard.c
KERNEL_OBJS := $(patsubst kernel/%.c,build/kernel/%.o,$(KERNEL_SRCS)) build/kernel/interrupts.o
KERNEL_LD   := kernel/linker.ld

STAGE1_BIN := build/stage1.bin
STAGE2_BIN := build/stage2.bin
KERNEL_ELF := build/kernel.elf

KERNEL_SECTOR := 40

all: $(IMG)

build:
	mkdir -p build build/kernel

$(STAGE1_BIN): $(STAGE1) | build
	$(ASM) $(ASMFLAGS) $< -o $@

$(STAGE2_BIN): $(STAGE2) bootloader/gdt.asm bootloader/paging.asm | build
	$(ASM) $(ASMFLAGS) $< -o $@

build/kernel/%.o: kernel/%.c | build
	$(CC) $(CFLAGS) $< -o $@

build/kernel/interrupts.o: kernel/interrupts.asm | build
	$(ASM) -f elf64 $< -o $@

$(KERNEL_ELF): $(KERNEL_OBJS) $(KERNEL_LD)
	$(LD) $(LDFLAGS) -o $@ $(KERNEL_OBJS)

$(IMG): $(STAGE1_BIN) $(STAGE2_BIN) $(KERNEL_ELF)
	dd if=/dev/zero of=$@ bs=512 count=2880 2>/dev/null
	dd if=$(STAGE1_BIN) of=$@ bs=512 seek=0 conv=notrunc 2>/dev/null
	dd if=$(STAGE2_BIN) of=$@ bs=512 seek=1 conv=notrunc 2>/dev/null
	dd if=$(KERNEL_ELF) of=$@ bs=512 seek=$(KERNEL_SECTOR) conv=notrunc 2>/dev/null

run: $(IMG)
	qemu-system-x86_64 -drive format=raw,file=$(IMG)

clean:
	rm -rf build $(IMG)

.PHONY: all build run clean