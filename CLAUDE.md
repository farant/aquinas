Don't need to delete any .stml files

## Troubleshooting

### Stuck at BIOS / Boot Loop
If the OS is stuck at BIOS or in a boot loop, **ALWAYS check that the bootloader is loading enough sectors for the kernel size**. 
- Check kernel size in build output
- Check boot.asm line: `mov ax, 0x020A` (where 0A = 10 sectors = 5KB)
- Increase sector count if kernel has grown beyond current allocation
