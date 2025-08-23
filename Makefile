# Aquinas OS Makefile

# Tools
AS = nasm
CC = x86_64-elf-gcc
LD = x86_64-elf-ld
QEMU = qemu-system-x86_64

# Directories
SRC_DIR = src
BUILD_DIR = build
BOOT_DIR = $(SRC_DIR)/boot
KERNEL_DIR = $(SRC_DIR)/kernel

# Flags
CFLAGS = -m32 -ffreestanding -fno-builtin -nostdlib -fno-pic -fno-pie -O2
LDFLAGS = -m elf_i386 -T $(SRC_DIR)/linker.ld -nostdlib

# Source files
BOOT_SRC = $(BOOT_DIR)/boot.asm
KERNEL_ENTRY_SRC = $(KERNEL_DIR)/kernel_entry.asm
KERNEL_C_SRCS = $(KERNEL_DIR)/kernel.c $(KERNEL_DIR)/serial.c

# Build files
BOOT_BIN = $(BUILD_DIR)/boot.bin
KERNEL_ENTRY_OBJ = $(BUILD_DIR)/kernel_entry.o
KERNEL_C_OBJS = $(BUILD_DIR)/kernel.o $(BUILD_DIR)/serial.o
KERNEL_BIN = $(BUILD_DIR)/kernel.bin
OS_IMG = $(BUILD_DIR)/aquinas.img

# Default target
all: $(BUILD_DIR) $(OS_IMG)

# Create build directory
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Build bootloader
$(BOOT_BIN): $(BOOT_SRC)
	$(AS) -f bin $< -o $@

# Build kernel entry
$(KERNEL_ENTRY_OBJ): $(KERNEL_ENTRY_SRC)
	$(AS) -f elf32 $< -o $@

# Build kernel C files
$(BUILD_DIR)/%.o: $(KERNEL_DIR)/%.c
	$(CC) $(CFLAGS) -I$(KERNEL_DIR) -c $< -o $@

# Link kernel
$(KERNEL_BIN): $(KERNEL_ENTRY_OBJ) $(KERNEL_C_OBJS)
	$(LD) $(LDFLAGS) $^ -o $@

# Create OS image
$(OS_IMG): $(BOOT_BIN) $(KERNEL_BIN)
	dd if=/dev/zero of=$@ bs=512 count=2880 2>/dev/null
	dd if=$(BOOT_BIN) of=$@ bs=512 conv=notrunc 2>/dev/null
	dd if=$(KERNEL_BIN) of=$@ bs=512 seek=1 conv=notrunc 2>/dev/null
	@echo "================================"
	@echo "Boot sector: $$(wc -c < $(BOOT_BIN)) bytes"
	@echo "Kernel size: $$(wc -c < $(KERNEL_BIN)) bytes"
	@echo "Image created: $(OS_IMG)"
	@echo "================================"

# Run the OS with both mouse and debug output
run: $(OS_IMG)
	$(QEMU) -drive file=$(OS_IMG),format=raw,if=floppy -m 128M -display cocoa,zoom-to-fit=on -full-screen -serial msmouse -serial stdio

# Run with only debug output (no mouse)
run-debug: $(OS_IMG)
	$(QEMU) -drive file=$(OS_IMG),format=raw,if=floppy -m 128M -serial stdio

# Debug mode
debug: $(OS_IMG)
	$(QEMU) -drive file=$(OS_IMG),format=raw,if=floppy -m 128M -d int,cpu_reset -no-reboot

# Clean build files
clean:
	rm -rf $(BUILD_DIR)

# Clean everything including editor files
distclean: clean
	rm -f *~ *.swp .DS_Store

.PHONY: all run debug clean distclean
