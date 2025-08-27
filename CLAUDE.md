Don't need to delete any .stml files

## AI Suggestions

When you have ideas for improvements or things to work on beyond what the user has explicitly requested (it's okay to add these even if you are in the middle of working on something else), add them to `AI_SUGGESTIONS.md`. 

If the user says they accept a suggestion, move it to `AI_SUGGESTIONS_ACCEPTED.md` to track approved ideas that can be worked on.

## Bug Tracking

When you discover bugs while working on the codebase, document them in `BUGS.md`. Once a bug is fixed, move it to the end of `BUGS_FIXED.md`. This gives us a historical record.

## Comment Philosophy
This project follows Antirez's comment philosophy (from Redis). Read about this philosophy in literature/antirez-comment-philosophy.md

## Running the OS

## Make Targets
- `make` 
- `make run` 
- `make clean` 

## Debug Targets (for troubleshooting crashes)
- `make debug` - Basic debugging with interrupt/exception tracking
- `make debug-cpu` - Includes CPU register dumps
- `make debug-trace` - Includes instruction disassembly
- `make debug-all` - Maximum verbosity

### Understanding Debug Output

When using debug targets, look for:

**Triple Faults:**
- Exception (e.g., #GP, #PF) → Double Fault (#DF) → Triple Fault → CPU Reset
- With `-no-reboot`, QEMU will halt showing the last state

**Common Exception Codes:**
- `check_exception old: 0xffffffff new 0xd` - General Protection Fault (#GP)
- `check_exception old: 0xffffffff new 0xe` - Page Fault (#PF)
- `check_exception old: 0xffffffff new 0x6` - Invalid Opcode (#UD)
- `check_exception old: 0xd new 0x8` - Double Fault (GPF led to DF)

**CPU State Fields:**
- `EIP` - Instruction pointer (where crash occurred)
- `ESP` - Stack pointer
- `CR0` - Control register (check protection enable bit)
- `CR2` - Page fault linear address
- `CR3` - Page directory base (if paging enabled)

## Debugging

### UART Serial Debug Output
The kernel has UART debug output via COM2 (0x2F8) that can be used for debugging without interfering with the mouse (which uses COM1).

**Available functions:**
- `serial_write_string(const char *str)` 
- `serial_write_char(char c)` 
- `serial_write_hex(unsigned int value)` 


## Troubleshooting

### Stuck at BIOS / Boot Loop / No Input / Blank Screen in Graphics Mode
If the OS is stuck at BIOS, in a boot loop, input (keyboard/mouse) stops working, or graphics mode shows blank/incomplete display, **ALWAYS check that the bootloader is loading enough sectors for the kernel size**. 
- Check kernel size in build output
- Check boot.asm line: `mov ax, 0x02A0` (where A0 = 160 sectors = 80KB)
- Increase sector count if kernel has grown beyond current allocation
- Current allocation: 160 sectors (80KB) as of DISPI implementation

