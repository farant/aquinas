Don't need to delete any .stml files

## Code Style & Comments

### Comment Philosophy
This project follows Antirez's comment philosophy (from Redis). Comments should reduce cognitive load and make code accessible to more developers.

**Good comment types to use:**
1. **Function comments** - Describe what a function does, its parameters, and return values
2. **Design comments** - Explain overall architecture and design decisions at the top of files
3. **Why comments** - Explain WHY code does something, especially when it's not obvious
4. **Teacher comments** - Explain domain knowledge (math, protocols, algorithms) that readers might not know
5. **Guide comments** - Help readers navigate complex code sections (but avoid trivial ones)
6. **Checklist comments** - Remind developers what else needs updating when changing something

**Example of a good "why" comment:**
```c
/* We accumulate mouse movements instead of applying them directly
 * because serial mice report very small increments. Without accumulation,
 * slow mouse movements would be ignored. */
accumulated_dx += dx;
```

**Example of a good "teacher" comment:**
```c
/* Microsoft Serial Mouse Protocol sends 3-byte packets:
 * Byte 0: 01LR YYyy XXxx (L=left button, R=right, high bits of movement)
 * Byte 1: 00XX XXXX (X movement, 6 bits)
 * Byte 2: 00YY YYYY (Y movement, 6 bits)
 * First byte has bit 6 set, bit 7 clear for resynchronization. */
```

**Avoid:**
- Trivial comments that just restate the code
- Backup comments (commented-out old code)
- Excessive TODO/FIXME comments

Remember: Comments are for communicating with future readers (including yourself). They should lower cognitive load, not increase it.

## Running the OS

### Make Targets
- `make` - Build the OS image
- `make run` - Run the OS with mouse support and debug output to terminal
- `make run-debug` - Run with only debug output (no mouse support)
- `make clean` - Clean build files

### Running in Background
**For Claude Code**: When using the Bash tool with `run_in_background: true`, the process runs in background and output can be monitored with the BashOutput tool. This is preferred for Claude Code as it allows better control and monitoring of the process. Can use this with `make run`.

**To stop QEMU**: Use `pkill -f qemu-system-x86_64`

## Debugging

### UART Serial Debug Output
The kernel has UART debug output via COM2 (0x2F8) that can be used for debugging without interfering with the mouse (which uses COM1).

**Available functions:**
- `serial_write_string(const char *str)` - Write a string to debug console
- `serial_write_char(char c)` - Write a single character
- `serial_write_hex(unsigned int value)` - Write a hex value (useful for addresses/values)

**How it works:**
- COM1 (0x3F8) is used for the serial mouse input
- COM2 (0x2F8) is used for debug output
- Run with `make run` to see debug output in terminal while using the OS
- The debug output goes to stdio in QEMU

**Example usage:**
```c
serial_write_string("Debug: Variable value is ");
serial_write_hex(some_variable);
serial_write_string("\n");
```

## Troubleshooting

### Stuck at BIOS / Boot Loop
If the OS is stuck at BIOS or in a boot loop, **ALWAYS check that the bootloader is loading enough sectors for the kernel size**. 
- Check kernel size in build output
- Check boot.asm line: `mov ax, 0x020A` (where 0A = 10 sectors = 5KB)
- Increase sector count if kernel has grown beyond current allocation
