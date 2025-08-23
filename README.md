# Aquinas OS

A simple 32-bit protected mode operating system written in assembly and C.

## Project Structure

```
aquinas/
├── src/                    # Source code
│   ├── boot/              # Bootloader
│   │   └── boot.asm       # Boot sector (loads kernel, switches to protected mode)
│   ├── kernel/            # Kernel code  
│   │   ├── kernel_entry.asm  # Assembly entry point
│   │   └── kernel.c          # Main kernel code
│   └── linker.ld          # Linker script
├── build/                 # Build outputs (gitignored)
│   ├── *.o               # Object files
│   ├── *.bin             # Binary files
│   └── aquinas.img       # Final bootable image
├── Makefile              # Build system
├── README.md             # This file
└── .gitignore           # Git ignore rules
```

## Building

Requirements:
- `nasm` - Assembler
- `x86_64-elf-gcc` - Cross compiler
- `x86_64-elf-ld` - Linker
- `qemu-system-x86_64` - Emulator

On macOS:
```bash
brew install nasm
brew install x86_64-elf-gcc
brew install qemu
```

## Usage

```bash
make        # Build the OS
make run    # Build and run in QEMU
make clean  # Clean build files
make debug  # Run with debug output
```

## Features

- 32-bit protected mode
- VGA text mode output (80x25, blue background)
- Kernel written in C
- Flat memory model (no paging)
- Loads at address 0x8000

## Memory Map

- `0x7C00` - Boot sector loaded by BIOS
- `0x8000` - Kernel loaded by bootloader
- `0x90000` - Stack
- `0xB8000` - VGA text buffer

## How It Works

1. BIOS loads boot sector to `0x7C00`
2. Bootloader loads kernel from disk to `0x8000`
3. Bootloader switches CPU to 32-bit protected mode
4. Bootloader jumps to kernel entry point
5. Kernel initializes and displays messages