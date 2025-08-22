/* linker.ld - Linker script for the kernel */

OUTPUT_FORMAT("binary")
ENTRY(_start)

SECTIONS
{
    . = 0x8000;
    
    .text : {
        *(.text)
    }
    
    .rodata : {
        *(.rodata)
    }
    
    .data : {
        *(.data)
    }
    
    .bss : {
        *(.bss)
    }
}
