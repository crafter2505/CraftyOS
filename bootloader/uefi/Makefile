CC = x86_64-w64-mingw32-gcc
LD = x86_64-w64-mingw32-ld

CFLAGS = -std=c11 \
         -ffreestanding \
         -fno-builtin \
         -fno-stack-protector \
         -fshort-wchar \
         -mno-red-zone \
         -Wall \
         -Wextra \
         -Werror \
         -I.

LDFLAGS = -e efi_main \
          --file-alignment 16 \
          --subsystem 10

IMAGE = ../../crafty-uefi.img
EFI_DIR = /usr/share/OVMF
OVMF_CODE ?= $(EFI_DIR)/OVMF_CODE_4M.fd
OVMF_VARS ?= $(EFI_DIR)/OVMF_VARS_4M.fd
OVMF_VARS_COPY = ../../build/OVMF_VARS_4M.fd

.PHONY: all image verify run clean

all: BOOTX64.EFI

main.obj: main.c efi.h
	$(CC) $(CFLAGS) -c main.c -o $@

BOOTX64.EFI: main.obj
	$(LD) $(LDFLAGS) $< -o $@

image: BOOTX64.EFI
	dd if=/dev/zero of=$(IMAGE) bs=1M count=64
	mformat -i $(IMAGE) -F ::
	mmd -i $(IMAGE) ::/EFI
	mmd -i $(IMAGE) ::/EFI/BOOT
	mcopy -i $(IMAGE) BOOTX64.EFI ::/EFI/BOOT/BOOTX64.EFI

verify: image
	mdir -i $(IMAGE) ::/EFI/BOOT

run: verify
	mkdir -p ../../build
	cp $(OVMF_VARS) $(OVMF_VARS_COPY)
	qemu-system-x86_64 \
		-drive if=pflash,format=raw,readonly=on,file=$(OVMF_CODE) \
		-drive if=pflash,format=raw,file=$(OVMF_VARS_COPY) \
		-drive format=raw,file=$(IMAGE) \
		-serial stdio

clean:
	rm -f main.obj BOOTX64.EFI $(IMAGE) $(OVMF_VARS_COPY)
