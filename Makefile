# Makefile for x86_64 OS (macOS version)

# Tools - using cross-compiler for macOS
CC = x86_64-elf-gcc
AS = nasm
LD = x86_64-elf-ld
OBJCOPY = x86_64-elf-objcopy
QEMU = qemu-system-x86_64

# Flags
CFLAGS = -ffreestanding -fno-builtin -fno-stack-protector \
         -nostdlib -nodefaultlibs -m64 -mcmodel=large \
         -mno-red-zone -mno-mmx -mno-sse -mno-sse2 \
         -std=c89 -O2 -Wall -Wextra
ASFLAGS = -f elf64
LDFLAGS = -T linker.ld -nostdlib

# Files
BOOT_SRC = boot.asm
KERNEL_ENTRY_SRC = kernel_entry.asm
KERNEL_SRC = kernel.c
KERNEL_OBJ = kernel.o
KERNEL_ENTRY_OBJ = kernel_entry.o
KERNEL_BIN = kernel.bin
BOOT_BIN = boot.bin
OS_IMG = os.img

# Targets
all: $(OS_IMG)

$(BOOT_BIN): $(BOOT_SRC)
	nasm -f bin $(BOOT_SRC) -o $(BOOT_BIN)

$(KERNEL_ENTRY_OBJ): $(KERNEL_ENTRY_SRC)
	$(AS) $(ASFLAGS) $(KERNEL_ENTRY_SRC) -o $(KERNEL_ENTRY_OBJ)

$(KERNEL_OBJ): $(KERNEL_SRC)
	$(CC) $(CFLAGS) -c $(KERNEL_SRC) -o $(KERNEL_OBJ)

$(KERNEL_BIN): $(KERNEL_ENTRY_OBJ) $(KERNEL_OBJ) linker.ld
	$(LD) $(LDFLAGS) $(KERNEL_ENTRY_OBJ) $(KERNEL_OBJ) -o $(KERNEL_BIN)

$(OS_IMG): $(BOOT_BIN) $(KERNEL_BIN)
	# Create a 1.44MB floppy image
	dd if=/dev/zero of=$(OS_IMG) bs=512 count=2880 2>/dev/null
	dd if=$(BOOT_BIN) of=$(OS_IMG) bs=512 conv=notrunc 2>/dev/null
	dd if=$(KERNEL_BIN) of=$(OS_IMG) bs=512 seek=1 conv=notrunc 2>/dev/null
	@echo "Boot sector size: $$(wc -c < $(BOOT_BIN)) bytes"
	@echo "Kernel size: $$(wc -c < $(KERNEL_BIN)) bytes"
	@echo "Kernel sectors: $$(( ($$(wc -c < $(KERNEL_BIN)) + 511) / 512 ))"

run: $(OS_IMG)
	$(QEMU) -drive file=$(OS_IMG),format=raw,if=floppy -m 128M

run-monitor: $(OS_IMG)
	$(QEMU) -drive file=$(OS_IMG),format=raw,if=floppy -m 128M -monitor stdio

run-debug: $(OS_IMG)
	$(QEMU) -drive file=$(OS_IMG),format=raw,if=floppy -m 128M -d int,cpu_reset -no-reboot -no-shutdown

gdb: $(OS_IMG)
	@echo "Starting QEMU in debug mode..."
	@echo "In another terminal, run: x86_64-elf-gdb"
	@echo "Then in GDB: target remote localhost:1234"
	$(QEMU) -drive file=$(OS_IMG),format=raw,if=floppy -m 128M -s -S

clean:
	rm -f *.o *.bin *.img

# Check if tools are installed
check-tools:
	@which x86_64-elf-gcc > /dev/null || (echo "Error: x86_64-elf-gcc not found. Install with: brew install x86_64-elf-gcc" && false)
	@which x86_64-elf-ld > /dev/null || (echo "Error: x86_64-elf-ld not found. Install with: brew install x86_64-elf-binutils" && false)
	@which nasm > /dev/null || (echo "Error: nasm not found. Install with: brew install nasm" && false)
	@which qemu-system-x86_64 > /dev/null || (echo "Error: qemu not found. Install with: brew install qemu" && false)
	@echo "All required tools are installed!"

.PHONY: all run run-debug gdb clean check-tools
