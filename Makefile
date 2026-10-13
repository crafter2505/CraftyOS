ASM      := nasm
ASMFLAGS := -f bin
CC       := gcc
CFLAGS   := -m64 -std=c11 -mcmodel=kernel -ffreestanding -fno-pic -mno-red-zone -fno-stack-protector -nostdlib -Wall -Wextra -Ikernel -Ikernel/mm -Ikernel/core -Ikernel/drivers -Ikernel/lib -Ikernel/arch -c
LD       := ld
LDFLAGS  := -m elf_x86_64 -T kernel/linker.ld -nostdlib -static

IMG        := CraftyOS.img
LOG 	   := qemu.log 
STAGE1     := bootloader/stage1.asm
STAGE2     := bootloader/stage2.asm

KERNEL_SRCS := $(shell find kernel -type f -name '*.c' -print -o -type f -name '*.asm' -print)
KERNEL_C_SRCS := $(filter %.c,$(KERNEL_SRCS))
KERNEL_ASM_SRCS := $(filter %.asm,$(KERNEL_SRCS))
KERNEL_OBJS := $(patsubst kernel/%.c,build/kernel/%.o,$(KERNEL_C_SRCS)) \
               $(patsubst kernel/%.asm,build/kernel/%.o,$(KERNEL_ASM_SRCS))
KERNEL_LD   := kernel/linker.ld

STAGE1_BIN := build/stage1.bin
STAGE2_BIN := build/stage2.bin
KERNEL_ELF := build/kernel.elf

KERNEL_SECTOR := 40

all: $(IMG)

build:
	mkdir -p build

$(STAGE1_BIN): $(STAGE1) | build
	$(ASM) $(ASMFLAGS) $< -o $@

$(STAGE2_BIN): $(STAGE2) bootloader/gdt.asm | build
	$(ASM) $(ASMFLAGS) $< -o $@

build/kernel/%.o: kernel/%.c Makefile | build
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $< -o $@

build/kernel/%.o: kernel/%.asm | build
	mkdir -p $(dir $@)
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

runlog: $(IMG)
	qemu-system-x86_64 -drive format=raw,file=$(IMG) -d int,cpu_reset -D $(LOG)

remake:
	$(MAKE) clean
	$(MAKE) all

clean:
	rm -rf build $(IMG) $(LOG)

.PHONY: all build run runlog remake clean