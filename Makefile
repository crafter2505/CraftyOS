ASM      := nasm
ASMFLAGS := -f bin
CC       := gcc
CFLAGS   := -m64 -std=c11 -mcmodel=kernel -ffreestanding -fno-pic -mno-red-zone -fno-stack-protector -nostdlib -Wall -Wextra -Ikernel -Ikernel/mm -Ikernel/core -Ikernel/drivers -Ikernel/lib -Ikernel/arch -Ikernel/fs -Ikernel/shell -c
LD       := ld
LDFLAGS  := -m elf_x86_64 -T kernel/linker.ld -nostdlib -static

IMG        			:= CraftyOS.img
UEFI_DIR   			:= bootloader/uefi
UEFI_EFI   			:= $(UEFI_DIR)/BOOTX64.EFI
UEFI_IMG   			:= crafty-uefi.img
BIOS_LOG        	:= bios.log
BIOS_SERIAL_LOG 	:= bios-serial.log
UEFI_LOG 			:= uefi.log
UEFI_SERIAL_LOG 	:= uefi-serial.log
OVMF_DIR   			:= /usr/share/OVMF
OVMF_CODE  			?= $(OVMF_DIR)/OVMF_CODE_4M.fd
OVMF_VARS  			?= $(OVMF_DIR)/OVMF_VARS_4M.fd
OVMF_VARS_COPY 		:= build/OVMF_VARS_4M.fd
STAGE1     			:= bootloader/stage1.asm
STAGE2     			:= bootloader/stage2.asm

KERNEL_SRCS 	:= $(shell find kernel -type f -name '*.c' -print -o -type f -name '*.asm' -print)
KERNEL_C_SRCS 	:= $(filter %.c,$(KERNEL_SRCS))
KERNEL_ASM_SRCS := $(filter %.asm,$(KERNEL_SRCS))
KERNEL_OBJS 	:= $(patsubst kernel/%.c,build/kernel/%.o,$(KERNEL_C_SRCS)) \
               	   $(patsubst kernel/%.asm,build/kernel/%.o,$(KERNEL_ASM_SRCS))
KERNEL_LD   	:= kernel/linker.ld

STAGE1_BIN := build/stage1.bin
STAGE2_BIN := build/stage2.bin
KERNEL_ELF := build/kernel.elf
USER_HELLO := build/user/hello.elf

KERNEL_SECTOR := 40

.DEFAULT_GOAL := parallel-all

all: $(IMG) $(UEFI_IMG)

.PHONY: parallel-all
parallel-all:
	+$(MAKE) $(if $(filter -j% --jobs%,$(MAKEFLAGS)),,-j$(shell nproc)) all

build:
	mkdir -p build

$(STAGE1_BIN): $(STAGE1) | build
	$(ASM) $(ASMFLAGS) $< -o $@

$(STAGE2_BIN): $(STAGE2) bootloader/gdt.asm | build
	$(ASM) $(ASMFLAGS) $< -o $@

build/kernel/%.o: kernel/%.c Makefile | build
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP $< -o $@

-include $(KERNEL_OBJS:.o=.d)

build/kernel/%.o: kernel/%.asm | build
	mkdir -p $(dir $@)
	$(ASM) -f elf64 $< -o $@

build/kernel/kernel.o build/kernel/lib/memory_map.o build/kernel/mm/vmm.o: kernel/lib/boot_info.h kernel/lib/memory_map.h
build/kernel/mm/pmm.o build/kernel/mm/buddy.o: kernel/lib/memory_map.h
build/kernel/drivers/ahci.o: kernel/drivers/ahci.h kernel/drivers/pci.h kernel/mm/buddy.h kernel/mm/vmm.h kernel/mm/address_space.h
build/kernel/drivers/pci.o: kernel/drivers/pci.h kernel/arch/io.h
build/kernel/kernel.o: kernel/drivers/ahci.h

$(KERNEL_ELF): $(KERNEL_OBJS) $(KERNEL_LD)
	$(LD) $(LDFLAGS) -o $@ $(KERNEL_OBJS)

$(USER_HELLO): user/hello.c | build
	mkdir -p build/user
	gcc -nostdlib -static -ffreestanding -fno-pic \
	    -fno-stack-protector -fno-asynchronous-unwind-tables \
	    -mno-red-zone -fno-pie -mcmodel=large \
	    -Wl,-Ttext-segment=0x8000000000 \
	    -Wl,-e,_start \
	    -Wl,-z,max-page-size=0x1000 \
	    $< -o $@

$(IMG): $(STAGE1_BIN) $(STAGE2_BIN) $(KERNEL_ELF)
	dd if=/dev/zero of=$@ bs=512 count=2880 2>/dev/null
	dd if=$(STAGE1_BIN) of=$@ bs=512 seek=0 conv=notrunc 2>/dev/null
	dd if=$(STAGE2_BIN) of=$@ bs=512 seek=1 conv=notrunc 2>/dev/null
	dd if=$(KERNEL_ELF) of=$@ bs=512 seek=$(KERNEL_SECTOR) conv=notrunc 2>/dev/null

$(UEFI_EFI): $(UEFI_DIR)/main.c $(UEFI_DIR)/efi.h $(UEFI_DIR)/elf.h \
             kernel/lib/boot_info.h $(UEFI_DIR)/Makefile
	$(MAKE) -C $(UEFI_DIR)

$(UEFI_IMG): $(UEFI_EFI) $(KERNEL_ELF) $(USER_HELLO)
	dd if=/dev/zero of=$@ bs=1M count=64
	mformat -i $@ -F ::
	mmd -i $@ ::/EFI
	mmd -i $@ ::/EFI/BOOT
	mcopy -i $@ $(UEFI_EFI) ::/EFI/BOOT/BOOTX64.EFI
	mcopy -i $@ $(KERNEL_ELF) ::/EFI/BOOT/kernel.elf
	mcopy -i $@ $(USER_HELLO) ::/EFI/BOOT/hello.elf
	printf 'hello from a long name\n' > /tmp/longfilename.txt
	mcopy -i $@ /tmp/longfilename.txt ::/longfilename.txt

$(OVMF_VARS_COPY): $(OVMF_VARS) | build
	cp $< $@

.PHONY: uefi verify-uefi run-uefi
uefi: $(UEFI_IMG)

verify-uefi: $(UEFI_IMG)
	mdir -i $(UEFI_IMG) ::/EFI/BOOT
	mdir -i $(UEFI_IMG) ::/

run: $(IMG)
	qemu-system-x86_64 \
		-drive format=raw,file=$(IMG) \
		-serial file:$(BIOS_SERIAL_LOG)

run-uefi: $(UEFI_IMG) $(OVMF_VARS_COPY)
	qemu-system-x86_64 \
		-drive if=pflash,format=raw,readonly=on,file=$(OVMF_CODE) \
		-drive if=pflash,format=raw,file=$(OVMF_VARS_COPY) \
		-drive format=raw,file=$(UEFI_IMG),if=none,id=disk0 \
		-device ahci,id=ahci \
		-device ide-hd,drive=disk0,bus=ahci.0 \
		-vga std \
		-display gtk,gl=off \
		-serial file:$(UEFI_SERIAL_LOG)
	@echo "--- $(UEFI_SERIAL_LOG) ---"
	@tail -40 $(UEFI_SERIAL_LOG)

runlog: $(IMG)
	qemu-system-x86_64 \
		-drive format=raw,file=$(IMG) \
		-serial file:$(BIOS_SERIAL_LOG) \
		-d int,cpu_reset -D $(BIOS_LOG)

runlog-uefi: $(UEFI_IMG) $(OVMF_VARS_COPY)
	qemu-system-x86_64 \
		-drive if=pflash,format=raw,readonly=on,file=$(OVMF_CODE) \
		-drive if=pflash,format=raw,file=$(OVMF_VARS_COPY) \
		-drive format=raw,file=$(UEFI_IMG),if=none,id=disk0 \
		-device ahci,id=ahci \
		-device ide-hd,drive=disk0,bus=ahci.0 \
		-vga std \
		-display gtk,gl=off \
		-serial file:$(UEFI_SERIAL_LOG) \
		-d int,cpu_reset -D $(UEFI_LOG)
	@echo "--- $(UEFI_SERIAL_LOG) ---"
	@tail -40 $(UEFI_SERIAL_LOG)

remake:
	$(MAKE) clean
	$(MAKE) parallel-all

clean:
	rm -rf build $(IMG) $(BIOS_LOG) $(BIOS_SERIAL_LOG) $(UEFI_LOG) $(UEFI_SERIAL_LOG)
	$(MAKE) -C $(UEFI_DIR) clean

.PHONY: all build run runlog run-uefi runlog-uefi remake clean