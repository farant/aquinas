/* AquinasOS Kernel */

#define VGA_BUFFER ((unsigned short*)0xB8000)
#define VGA_WIDTH 80
#define VGA_HEIGHT 25
#define VGA_COLOR 0x1F00  /* Blue background, white text */

/* Global cursor position */
int cursor_pos = 0;

/* VGA output functions */
void putchar(char c) {
    if (c == '\n') {
        cursor_pos = ((cursor_pos / VGA_WIDTH) + 1) * VGA_WIDTH;
        return;
    } else if (c == '\b') {  /* Backspace */
        if (cursor_pos > 0) {
            cursor_pos--;
            VGA_BUFFER[cursor_pos] = VGA_COLOR | ' ';
        }
        return;
    }
    
    /* Regular character */
    VGA_BUFFER[cursor_pos++] = VGA_COLOR | c;
    
    /* Scroll if needed */
    if (cursor_pos >= VGA_WIDTH * VGA_HEIGHT) {
        /* Move everything up one line */
        for (int i = 0; i < VGA_WIDTH * (VGA_HEIGHT - 1); i++) {
            VGA_BUFFER[i] = VGA_BUFFER[i + VGA_WIDTH];
        }
        /* Clear last line */
        for (int i = VGA_WIDTH * (VGA_HEIGHT - 1); i < VGA_WIDTH * VGA_HEIGHT; i++) {
            VGA_BUFFER[i] = VGA_COLOR | ' ';
        }
        cursor_pos = VGA_WIDTH * (VGA_HEIGHT - 1);
    }
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

/* Read a byte from port */
static inline unsigned char inb(unsigned short port) {
    unsigned char ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "dN"(port));
    return ret;
}

/* Simple keyboard test */
char keyboard_getchar(void) {
    unsigned char status;
    char keycode;
    
    /* Simple scancode to ASCII for testing */
    static const char scancode_map[128] = {
        0, 27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
        '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
        0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
        0, '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,
        '*', 0, ' ', 0
    };
    
    /* Wait for key press */
    do {
        status = inb(0x64);
    } while (!(status & 1));
    
    keycode = inb(0x60);
    
    /* Only process key press (not release) */
    if (keycode < 128 && keycode > 0) {
        return scancode_map[(int)keycode];
    }
    return 0;
}

/* Kernel entry point */
void kernel_main(void) {
    char c;
    
    clear_screen();
    
    puts("AquinasOS Kernel v0.1\n");
    puts("================================\n");
    puts("Simple Keyboard Test\n");
    puts("Type characters (letters/numbers/space)\n");
    puts("Press ESC to halt\n\n");
    
    puts("> ");
    
    /* Simple keyboard echo loop */
    while (1) {
        c = keyboard_getchar();
        
        if (c == 27) {  /* ESC key */
            puts("\n\nSystem halted.\n");
            break;
        }
        
        if (c == '\n') {
            putchar('\n');
            puts("> ");
        } else if (c != 0) {
            putchar(c);
        }
    }
    
    /* Halt */
    while (1) {
        __asm__ volatile ("hlt");
    }
}
