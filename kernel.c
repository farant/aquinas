/* Working C kernel for the fixed bootloader */

#define VGA_BUFFER ((unsigned short*)0xB8000)
#define VGA_WIDTH 80
#define VGA_HEIGHT 25
#define VGA_COLOR 0x1F00  /* Blue background, white text */

static int cursor_pos = 0;

void putchar(char c) {
    if (c == '\n') {
        cursor_pos = ((cursor_pos / VGA_WIDTH) + 1) * VGA_WIDTH;
        return;
    }
    VGA_BUFFER[cursor_pos++] = VGA_COLOR | c;
}

void puts(const char* str) {
    while (*str) {
        putchar(*str++);
    }
}

void clear_screen(void) {
    for (int i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) {
        VGA_BUFFER[i] = VGA_COLOR | ' ';
    }
    cursor_pos = 0;
}

void kernel_main(void) {
    clear_screen();
    
    puts("C Kernel Running Successfully!\n");
    puts("================================\n\n");
    puts("Features:\n");
    puts("  - 32-bit protected mode\n");
    puts("  - VGA text output\n");
    puts("  - Stable, no crashes\n");
    puts("  - Loaded at 0x8000\n\n");
    puts("System halted.\n");
    
    /* Halt forever */
    while (1) {
        __asm__ volatile ("hlt");
    }
}