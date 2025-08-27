# Aquinas OS

A minimalist text editor operating system - boots directly into a multi-page text editor with mouse support.

Inspired by Acme, event sourcing, XML, object database filesystems, and graph databases.

## Project Structure

```
aquinas/
├── src/                          # Source code
│   ├── boot/                    # Bootloader
│   │   └── boot.asm             # Boot sector (loads kernel, switches to protected mode)
│   ├── kernel/                  # Kernel code  
│   │   ├── kernel_entry.asm     # Assembly entry point
│   │   ├── kernel.c             # Main kernel and event loop
│   │   ├── page.c/h             # Page management and navigation
│   │   ├── editor.c/h           # Text editing operations
│   │   ├── display.c/h          # Screen rendering and UI
│   │   ├── commands.c/h         # Command and link execution
│   │   ├── modes.c/h            # Editor mode management (Normal/Insert/Visual)
│   │   ├── input.c/h            # Keyboard and mouse input handling
│   │   ├── serial.c/h           # Serial port communication (mouse & debug)
│   │   ├── io.h                 # Port I/O functions
│   │   ├── vga.c/h              # VGA text mode implementation
│   │   ├── graphics.c/h         # VGA graphics mode (320x200 mode 12h)
│   │   ├── graphics_context.c/h # Graphics context system (clipping, patterns)
│   │   ├── grid.c/h             # Grid system for UI layout
│   │   ├── dispi.c/h            # DISPI/VBE graphics driver (640x480)
│   │   ├── dispi_demo.c/h       # DISPI graphics demonstration
│   │   ├── display_driver.c/h   # Display driver abstraction layer
│   │   ├── dispi_cursor.c/h     # Mouse cursor for DISPI mode
│   │   ├── view.c/h             # Hierarchical view system for UI
│   │   ├── layout.c/h           # Layout manager for screen regions
│   │   ├── pci.c/h              # PCI bus scanning for graphics devices
│   │   ├── font_6x8.h           # HP 100LX bitmap font
│   │   ├── text_renderer.c/h    # Text rendering for graphics modes
│   │   ├── mouse.h              # Mouse definitions
│   │   ├── memory.c/h           # Memory management
│   │   ├── timer.c/h            # Timer and timing functions
│   │   ├── timer_asm.asm        # Timer assembly helpers
│   │   └── rtc.c/h              # Real-time clock
│   └── linker.ld                # Linker script
├── build/                       # Build outputs (gitignored)
│   ├── *.o                      # Object files
│   ├── *.bin                    # Binary files
│   └── aquinas.img              # Final bootable image
├── literature/                  # Programming philosophy references
│   ├── antirez-comment-philosophy.md
│   ├── casey-muratori-big-oops.md
│   ├── charles-simonyi-intentional-programming.md
│   ├── eskil-steenberg-programming-c.md
│   ├── john-carmack-functional-programming.md
│   └── sean-barrett-writing-small-programs-in-c.md
├── tools/                       # Build tools
│   ├── stb_truetype.h          # TrueType font library
│   ├── ttf_to_c                # Font converter executable
│   └── ttf_to_c_stb.c          # Font converter source
├── Makefile                     # Build system
├── README.md                    # This file
├── CLAUDE.md                    # Development guidelines & instructions
├── AI_SUGGESTIONS.md            # AI-generated improvement ideas
├── AI_SUGGESTIONS_ACCEPTED.md   # Accepted AI suggestions
├── BUGS.md                      # Known bugs tracking
├── GOALS.md                     # Project goals and vision
├── UI-IDEAS.md                  # UI design ideas and concepts
├── notes.stml                   # Development notes
└── Px437_HP_100LX_6x8.ttf      # HP 100LX font file
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

### Graphics Modes

The system supports multiple graphics modes for visualization and demos:

#### VGA Mode 12h (320×200, 16 colors)
- **$graphics**: Launches VGA graphics demo with palette showcase and animations
- 4-plane architecture with palette-indexed colors
- Custom Aquinas palette optimized for grays, reds, golds, and cyans
- Mouse cursor with black outline for visibility

#### DISPI/VBE Mode (640×480, 256 colors)
- **$dispi**: Launches DISPI graphics demo with text rendering
- Linear framebuffer accessed via PCI detection
- Text rendering with both 6×8 and 9×16 (BIOS) fonts
- Interactive text input with blinking cursor
- Full mouse support with arrow cursor
- Smooth transition back to text mode

Interactive test modes (press in DISPI mode):
- **F**: Performance benchmark comparing regular vs optimized rectangle fills
- **G**: Graphics primitives test (toggles display of lines, circles, BIOS font)
- **R**: Grid system visualization (toggles grid overlay with mouse hover highlighting)
- **C**: Graphics context test (demonstrates clipping, translation, and pattern fills)

Performance optimizations:
- Double buffering for flicker-free rendering
- Dirty rectangle tracking (only redraws changed regions)
- 32-bit aligned memory operations for ~4x faster rectangle fills
- Automatic merging of overlapping dirty regions

Graphics primitives:
- Line drawing using Bresenham's algorithm
- Circle drawing using midpoint circle algorithm
- Filled circle drawing using scanline algorithm
- BIOS 9×16 font rendering from saved VGA font data
- Transparent blitting with color key support
- Pattern fills with 8×8 bitmap patterns

Graphics context system:
- Clipping regions to restrict drawing to defined bounds
- Translation to shift coordinate system origin
- Pattern fill modes with built-in patterns (checkerboard, stripes, dots)
- Context state management for colors and fill modes
- Multiple contexts can draw to different screen regions

Grid system (foundation for UI):
- 71×30 cell grid (9×16 pixels per cell, matching VGA text mode)
- 7×6 region grid (90×80 pixels per region, 10×5 cells each)
- Movable 10-pixel vertical bar between columns
- Coordinate conversion between pixels, cells, and regions
- Mouse hover cell highlighting in grid test mode

Both graphics modes feature:
- Real-time mouse cursor tracking
- Keyboard input handling (ESC to exit)
- Proper state preservation when returning to text mode

### View System and Layout Manager

The editor includes a hierarchical view system and layout manager for building complex UI components:

#### View System
- **Hierarchical structure**: Views can have parent-child relationships
- **Event handling**: Each view can handle keyboard and mouse events
- **Custom drawing**: Views implement their own draw methods using graphics context
- **Invalidation tracking**: Automatic tracking of which views need redrawing
- **Hit testing**: Find which view is under the mouse cursor
- **Focus management**: Track which view has keyboard focus

#### Layout Manager
- **7×6 Region Grid**: Screen divided into 7 columns × 6 rows of regions (90×80 pixels each)
- **Region-based positioning**: Views are positioned and sized in region units
- **Navigator-Target relationships**: Acme-style linked panes where one controls another
- **Layout types**:
  - Single: One view fills entire screen
  - Split: Navigator on left, target on right with optional divider bar
  - Custom: Arbitrary arrangement of regions
- **Vertical bar**: 10-pixel moveable divider between columns
- **Active region tracking**: Highlights the currently focused region

### Interactive Commands & Links

The editor supports Acme-inspired interactive elements that execute when clicked with the mouse:

#### Commands (start with $)
Commands perform actions and can insert output into the text:

- **$date**: Inserts the current date and time (MM/DD/YYYY HH:MM format)
- **$rename [name]**: Sets the name of the current page (appears in navigation bar)
- **$graphics**: Launches VGA mode 12h graphics demo
- **$dispi**: Launches DISPI/VBE graphics demo with text rendering
- **$layout**: Launches layout and view system demo showcasing UI components

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

## API Examples

### Creating a Simple View
```c
/* Create a view that fills 2x2 regions */
View *my_view = view_create(1, 1, 2, 2);

/* Set custom draw function */
my_view->draw = my_custom_draw;
my_view->handle_event = my_event_handler;

/* Add to layout */
layout_set_region_content(layout, 1, 1, 2, 2, my_view);
```

### Setting Up Split Layout
```c
/* Create navigator and target views */
View *navigator = view_create(0, 0, 3, 6);
View *target = view_create(3, 0, 4, 6);

/* Set up split layout with bar at column 3 */
layout_set_split(layout, navigator, target, 3);
```

## How It Works

### Boot Process
1. BIOS loads boot sector to `0x7C00`
2. Bootloader loads kernel from IDE hard drive to `0x8000` (144 sectors = 72KB)
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
- **Graphics drivers**: Abstracted display driver interface supporting both VGA and DISPI
- **Memory management**: Bump allocator with ~300KB allocated for double buffering
- **Bootloader**: Loads up to 144 sectors (72KB) of kernel code
- **UI Architecture**: Layered system from device drivers up to layout management:
  - Display drivers (VGA/DISPI abstraction)
  - Graphics primitives (lines, circles, rectangles)
  - Graphics context (clipping, translation, patterns)
  - Grid system (cells and regions)
  - View system (hierarchical UI components)
  - Layout manager (screen organization)
