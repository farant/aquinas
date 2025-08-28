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
CFLAGS = -m32 -std=c89 -pedantic -ffreestanding -fno-builtin -nostdlib -fno-pic -fno-pie -mno-sse -mno-sse2 -O2
LDFLAGS = -m elf_i386 -T $(SRC_DIR)/linker.ld -nostdlib

# Source files
BOOT_SRC = $(BOOT_DIR)/boot.asm
KERNEL_ENTRY_SRC = $(KERNEL_DIR)/kernel_entry.asm
KERNEL_C_SRCS = $(KERNEL_DIR)/kernel.c $(KERNEL_DIR)/serial.c $(KERNEL_DIR)/vga.c $(KERNEL_DIR)/timer.c $(KERNEL_DIR)/rtc.c $(KERNEL_DIR)/memory.c $(KERNEL_DIR)/graphics.c $(KERNEL_DIR)/dispi.c $(KERNEL_DIR)/display_driver.c $(KERNEL_DIR)/pci.c $(KERNEL_DIR)/dispi_cursor.c $(KERNEL_DIR)/grid.c $(KERNEL_DIR)/graphics_context.c $(KERNEL_DIR)/page.c $(KERNEL_DIR)/modes.c $(KERNEL_DIR)/display.c $(KERNEL_DIR)/commands.c $(KERNEL_DIR)/editor.c $(KERNEL_DIR)/input.c $(KERNEL_DIR)/mouse.c $(KERNEL_DIR)/dispi_init.c $(KERNEL_DIR)/dispi_demo.c $(KERNEL_DIR)/view.c $(KERNEL_DIR)/layout.c $(KERNEL_DIR)/layout_demo.c $(KERNEL_DIR)/ui_button.c $(KERNEL_DIR)/ui_label.c $(KERNEL_DIR)/ui_panel.c $(KERNEL_DIR)/ui_textinput.c $(KERNEL_DIR)/text_edit_base.c $(KERNEL_DIR)/ui_textarea.c $(KERNEL_DIR)/ui_demo.c

# Build files
BOOT_BIN = $(BUILD_DIR)/boot.bin
KERNEL_ENTRY_OBJ = $(BUILD_DIR)/kernel_entry.o
KERNEL_C_OBJS = $(BUILD_DIR)/kernel.o $(BUILD_DIR)/serial.o $(BUILD_DIR)/vga.o $(BUILD_DIR)/timer.o $(BUILD_DIR)/rtc.o $(BUILD_DIR)/memory.o $(BUILD_DIR)/graphics.o $(BUILD_DIR)/dispi.o $(BUILD_DIR)/display_driver.o $(BUILD_DIR)/pci.o $(BUILD_DIR)/dispi_cursor.o $(BUILD_DIR)/grid.o $(BUILD_DIR)/graphics_context.o $(BUILD_DIR)/page.o $(BUILD_DIR)/modes.o $(BUILD_DIR)/display.o $(BUILD_DIR)/commands.o $(BUILD_DIR)/editor.o $(BUILD_DIR)/input.o $(BUILD_DIR)/mouse.o $(BUILD_DIR)/dispi_init.o $(BUILD_DIR)/dispi_demo.o $(BUILD_DIR)/view.o $(BUILD_DIR)/layout.o $(BUILD_DIR)/layout_demo.o $(BUILD_DIR)/ui_button.o $(BUILD_DIR)/ui_label.o $(BUILD_DIR)/ui_panel.o $(BUILD_DIR)/ui_textinput.o $(BUILD_DIR)/text_edit_base.o $(BUILD_DIR)/ui_textarea.o $(BUILD_DIR)/ui_demo.o
TIMER_ASM_OBJ = $(BUILD_DIR)/timer_asm.o
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

# Build timer assembly
$(TIMER_ASM_OBJ): $(KERNEL_DIR)/timer_asm.asm
	$(AS) -f elf32 $< -o $@

# Build kernel C files
$(BUILD_DIR)/%.o: $(KERNEL_DIR)/%.c
	$(CC) $(CFLAGS) -I$(KERNEL_DIR) -c $< -o $@

# Link kernel
$(KERNEL_BIN): $(KERNEL_ENTRY_OBJ) $(KERNEL_C_OBJS) $(TIMER_ASM_OBJ)
	$(LD) $(LDFLAGS) $^ -o $@

# Create OS image (10MB IDE disk instead of 1.44MB floppy)
$(OS_IMG): $(BOOT_BIN) $(KERNEL_BIN)
	dd if=/dev/zero of=$@ bs=1M count=10 2>/dev/null
	dd if=$(BOOT_BIN) of=$@ bs=512 conv=notrunc 2>/dev/null
	dd if=$(KERNEL_BIN) of=$@ bs=512 seek=1 conv=notrunc 2>/dev/null
	@echo "================================"
	@echo "Boot sector: $$(wc -c < $(BOOT_BIN)) bytes"
	@echo "Kernel size: $$(wc -c < $(KERNEL_BIN)) bytes"
	@echo "Image created: $(OS_IMG)"
	@echo "================================"

# Run the OS with both mouse and debug output (IDE disk)
run: $(OS_IMG)
	$(QEMU) -drive file=$(OS_IMG),format=raw -m 128M -display cocoa,zoom-to-fit=on -full-screen -serial msmouse -serial stdio

# Run without zoom-to-fit (more stable for mode switches)
run-stable: $(OS_IMG)
	$(QEMU) -drive file=$(OS_IMG),format=raw -m 128M -display cocoa -serial msmouse -serial stdio

# Run with only debug output (no mouse)
run-debug: $(OS_IMG)
	$(QEMU) -drive file=$(OS_IMG),format=raw -m 128M -serial stdio

# Debug mode - shows interrupts and CPU resets, halts on triple fault
debug: $(OS_IMG)
	$(QEMU) -drive file=$(OS_IMG),format=raw -m 128M -no-reboot -no-shutdown -d int,cpu_reset,guest_errors -display cocoa,zoom-to-fit=on -full-screen -serial msmouse -serial stdio

# Debug with CPU state - also shows CPU register dumps
debug-cpu: $(OS_IMG)
	$(QEMU) -drive file=$(OS_IMG),format=raw -m 128M -no-reboot -no-shutdown -d int,cpu_reset,cpu,guest_errors -display cocoa,zoom-to-fit=on -full-screen -serial msmouse -serial stdio


# Debug with instruction trace - WARNING: Very verbose!
debug-trace: $(OS_IMG)
	$(QEMU) -drive file=$(OS_IMG),format=raw -m 128M -no-reboot -no-shutdown -d int,cpu_reset,in_asm,guest_errors -display cocoa,zoom-to-fit=on -full-screen -serial msmouse -serial stdio


# Debug everything - WARNING: Extremely verbose!
debug-all: $(OS_IMG)
	$(QEMU) -drive file=$(OS_IMG),format=raw -m 128M -no-reboot -no-shutdown -d int,cpu_reset,cpu,exec,in_asm,guest_errors -display cocoa,zoom-to-fit=on -full-screen -serial msmouse -serial stdio


# Clean build files
clean:
	rm -rf $(BUILD_DIR)

# Clean everything including editor files
distclean: clean
	rm -f *~ *.swp .DS_Store

.PHONY: all run run-debug debug debug-cpu debug-trace debug-all clean distclean
