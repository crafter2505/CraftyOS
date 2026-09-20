ASM      := nasm
ASMFLAGS := -f bin

IMG        := CraftyOS.img
STAGE1     := bootloader/stage1.asm
STAGE2     := bootloader/stage2.asm
STAGE1_BIN := build/stage1.bin
STAGE2_BIN := build/stage2.bin

all: $(IMG)

build:
	mkdir -p build

$(STAGE1_BIN): $(STAGE1) | build
	$(ASM) $(ASMFLAGS) $< -o $@

$(STAGE2_BIN): $(STAGE2) bootloader/gdt.asm bootloader/paging.asm | build
	$(ASM) $(ASMFLAGS) $< -o $@

$(IMG): $(STAGE1_BIN) $(STAGE2_BIN)
	dd if=/dev/zero of=$@ bs=512 count=2880 2>/dev/null
	dd if=$(STAGE1_BIN) of=$@ bs=512 seek=0 conv=notrunc 2>/dev/null
	dd if=$(STAGE2_BIN) of=$@ bs=512 seek=1 conv=notrunc 2>/dev/null

run: $(IMG)
	qemu-system-x86_64 -drive format=raw,file=$(IMG)

clean:
	rm -rf build $(IMG)

.PHONY: all build run clean