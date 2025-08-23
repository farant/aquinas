Don't need to delete any .stml files

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
