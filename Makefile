# Makefile for the working OS

# Tools
AS = nasm
CC = x86_64-elf-gcc
LD = x86_64-elf-ld
QEMU = qemu-system-x86_64

# Flags
CFLAGS = -m32 -ffreestanding -fno-builtin -nostdlib -fno-pic -fno-pie -O2
LDFLAGS = -m elf_i386 -T linker.ld -nostdlib

# Files
BOOT_BIN = boot.bin
KERNEL_ENTRY_OBJ = kernel_entry.o
KERNEL_C_OBJ = kernel.o
KERNEL_BIN = kernel.bin
OS_IMG = aquinas.img

all: $(OS_IMG)

$(BOOT_BIN): boot.asm
	$(AS) -f bin $< -o $@

$(KERNEL_ENTRY_OBJ): kernel_entry.asm
	$(AS) -f elf32 $< -o $@

$(KERNEL_C_OBJ): kernel.c
	$(CC) $(CFLAGS) -c $< -o $@

$(KERNEL_BIN): $(KERNEL_ENTRY_OBJ) $(KERNEL_C_OBJ) linker.ld
	$(LD) $(LDFLAGS) $(KERNEL_ENTRY_OBJ) $(KERNEL_C_OBJ) -o $@

$(OS_IMG): $(BOOT_BIN) $(KERNEL_BIN)
	dd if=/dev/zero of=$@ bs=512 count=2880 2>/dev/null
	dd if=$(BOOT_BIN) of=$@ bs=512 conv=notrunc 2>/dev/null
	dd if=$(KERNEL_BIN) of=$@ bs=512 seek=1 conv=notrunc 2>/dev/null
	@echo "================================"
	@echo "Boot sector: $$(wc -c < $(BOOT_BIN)) bytes"
	@echo "Kernel size: $$(wc -c < $(KERNEL_BIN)) bytes"
	@echo "Image created: $(OS_IMG)"
	@echo "================================"

run: $(OS_IMG)
	$(QEMU) -drive file=$(OS_IMG),format=raw,if=floppy -m 128M

debug: $(OS_IMG)
	$(QEMU) -drive file=$(OS_IMG),format=raw,if=floppy -m 128M -d int,cpu_reset -no-reboot

clean:
	rm -f *.o *.bin *.img

.PHONY: all run debug clean