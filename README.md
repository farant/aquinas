# Aquinas OS

A minimalist text editor operating system - boots directly into a multi-page text editor with mouse support.

Inspired by Acme, event sourcing, XML, object database filesystems, and graph databases.

## Project Structure

```
aquinas/
├── src/                      # Source code
│   ├── boot/                # Bootloader
│   │   └── boot.asm         # Boot sector (loads kernel, switches to protected mode)
│   ├── kernel/              # Kernel code  
│   │   ├── kernel_entry.asm # Assembly entry point
│   │   ├── kernel.c         # Main kernel code
│   │   ├── serial.c         # Serial port communication (mouse & debug)
│   │   ├── serial.h         # Serial port definitions
│   │   ├── io.h             # Port I/O functions
│   │   ├── vga.h            # VGA text mode definitions
│   │   └── mouse.h          # Mouse protocol definitions
│   └── linker.ld            # Linker script
├── build/                   # Build outputs (gitignored)
│   ├── *.o                  # Object files
│   ├── *.bin                # Binary files
│   └── aquinas.img          # Final bootable image
├── Makefile                 # Build system
├── README.md                # This file
├── CLAUDE.md                # Development guidelines & instructions
└── notes.stml               # Development notes
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

# Debug targets (for troubleshooting)
make debug       # Show interrupts and exceptions
make debug-cpu   # Include CPU register dumps  
make debug-trace # Include instruction trace (verbose)
make debug-all   # Maximum verbosity
```

## Features

### Core System
- 32-bit protected mode
- VGA text mode output (80x25, blue background)
- Kernel written in C
- Flat memory model (no paging)
- Loads at address 0x8000

### Text Editor
- **Page-based editing**: Each page holds one screen of text (24 lines × 80 characters)
- **Independent pages**: Each page has its own buffer and cursor position
- **Page naming**: Pages can be named using the $rename command
- **Auto-indentation**: Maintains indentation when pressing Enter
- **Tab support**: Tab key inserts actual tab characters (displayed as 2 spaces)

### Mouse Support
- Serial mouse via QEMU's `-serial msmouse` option
- Click on navigation buttons to switch pages
- Visual cursor indicator (green background)
- Smooth movement with accumulator-based precision

### Keyboard Features
- Full keyboard support with shift key for capitals and symbols
- Arrow keys for cursor movement within pages
- **Shift + Left/Right Arrow**: Navigate between pages (works in all modes)
- Backspace for character deletion
- Non-blocking input (keyboard and mouse work simultaneously)

### Vim Mode
The editor includes vim-style modal editing with three modes:

#### Normal Mode (default)
- **h/j/k/l**: Move cursor left/down/up/right (vim navigation)
- **w**: Move forward one word
- **b**: Move backward one word
- **$**: Move to end of line
- **^**: Move to first non-whitespace character of line
- **i**: Enter insert mode at cursor
- **a**: Enter insert mode after cursor (append)
- **o**: Insert new line below and enter insert mode (with auto-indent)
- **O**: Insert new line above and enter insert mode (with auto-indent)
- **v**: Enter visual mode for selection
- **x**: Delete character under cursor
- **dd**: Delete entire line
- **d$**: Delete from cursor to end of line
- **d^**: Delete from cursor to beginning of line (first non-whitespace)
- **dt[char]**: Delete from cursor up to (but not including) specified character
- **ESC** or **Ctrl+[**: Return to normal mode

#### Insert Mode
- Type normally to insert text
- **Arrow keys**: Move cursor
- **Backspace**: Delete character before cursor
- **Tab**: Insert tab character (displayed as 2 spaces)
- **Enter**: New line with auto-indentation
- **ESC**, **Ctrl+[**, or **fd** (typed quickly): Return to normal mode
- **Shift + Left/Right Arrow**: Navigate between pages

#### Visual Mode
- **h/j/k/l** or **Arrow keys**: Extend selection
- **ESC**: Return to normal mode (cancels selection)
- Text selection is highlighted in red

### Interactive Commands & Links

The editor supports Acme-inspired interactive elements that execute when clicked with the mouse:

#### Commands (start with $)
Commands perform actions and can insert output into the text:

- **$date**: Inserts the current date and time (MM/DD/YYYY HH:MM format)
- **$rename [name]**: Sets the name of the current page (appears in navigation bar)

When clicking a command, it intelligently handles output insertion:
- Uses existing whitespace when available
- Pushes text aside when necessary
- Adds spacing to separate output from following text

#### Links (start with #)
Links provide navigation between pages:

- **#1, #2, #3...**: Navigate to specific page numbers
- **#last-page**: Jump to the last page in the document  
- **#[page-name]**: Navigate to a page by its name (e.g., #my-notes)
- **#back**: Return to the previous page (maintains navigation history)

Navigation history is automatically tracked, allowing #back to retrace your steps through the pages you've visited.

## Memory Map

- `0x7C00` - Boot sector loaded by BIOS
- `0x8000` - Kernel loaded by bootloader
- `0xB8000` - VGA text buffer
- `0x200000` - Stack (2MB mark, grows downward)

## How It Works

### Boot Process
1. BIOS loads boot sector to `0x7C00`
2. Bootloader loads kernel from IDE hard drive to `0x8000` (30 sectors = 15KB)
3. Bootloader enables A20 line for >1MB memory access
4. Bootloader switches CPU to 32-bit protected mode
5. Bootloader jumps to kernel entry point
6. Kernel initializes hardware and starts text editor

### Text Editor Architecture
The editor uses a **page-based architecture** where each page is completely independent:

```c
typedef struct {
    char buffer[1920];     // 24 lines × 80 chars (one screen)
    int length;            // Current text length in this page
    int cursor_pos;        // Cursor position in this page
} Page;
```

- **No continuous buffer**: Pages don't overflow into each other
- **Per-page state**: Each page remembers its own cursor position
- **Page limit**: When a page is full, typing stops (no auto-advance)
- **Navigation bar**: Shows "Page X of Y" with [prev] and [next] buttons

### User Interface
```
┌─────────────────────────────────────────────────────────────────┐
│ INSERT: my-page  [prev] Page 1 of 3 [next]   ← Mode, name & nav│
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  Your text appears here...                                      │
│  Each page can hold 24 lines of 80 characters.                  │
│  _                              ← Cursor (blinking)             │
│                                                                  │
│                                ← Mouse cursor (green background) │
└─────────────────────────────────────────────────────────────────┘
```

The navigation bar displays:
- Current editing mode (NORMAL, INSERT, or VISUAL)
- Page name (if set via $rename)
- Page navigation controls and current page number

### Navigation Controls
- **Mouse**: Click [prev] or [next] buttons in navigation bar
- **Keyboard**: 
  - Shift + Left Arrow = Previous page
  - Shift + Right Arrow = Next page
  - Arrow keys = Move cursor within current page

### Technical Details
- **Non-blocking I/O**: Keyboard and mouse are polled, not interrupt-driven
- **Hardware cursor**: Uses VGA hardware cursor for text insertion point
- **Mouse protocol**: Microsoft 3-byte serial mouse packets
- **Scancode mapping**: Direct PS/2 scancode to ASCII conversion
- **Page storage**: Static array of 100 pages maximum
