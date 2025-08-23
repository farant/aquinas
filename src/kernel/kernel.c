/* AquinasOS Kernel */

#define VGA_BUFFER ((unsigned short*)0xB8000)
#define VGA_WIDTH 80
#define VGA_HEIGHT 25
#define VGA_COLOR 0x1F00  /* Blue background, white text */

/* Global cursor position */
int cursor_pos = 0;

/* Write a byte to port */
static inline void outb(unsigned short port, unsigned char val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "dN"(port));
}

/* Update hardware cursor position */
void update_cursor(void) {
    unsigned short pos = cursor_pos;
    
    /* Tell VGA board we're setting the high cursor byte */
    outb(0x3D4, 0x0E);
    outb(0x3D5, (unsigned char)((pos >> 8) & 0xFF));
    
    /* Tell VGA board we're setting the low cursor byte */
    outb(0x3D4, 0x0F);
    outb(0x3D5, (unsigned char)(pos & 0xFF));
}

/* VGA output functions */
void putchar(char c) {
    if (c == '\n') {
        cursor_pos = ((cursor_pos / VGA_WIDTH) + 1) * VGA_WIDTH;
        update_cursor();
        return;
    } else if (c == '\b') {  /* Backspace */
        if (cursor_pos > 0) {
            cursor_pos--;
            VGA_BUFFER[cursor_pos] = VGA_COLOR | ' ';
            update_cursor();
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
    
    update_cursor();
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
    update_cursor();
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
    unsigned char keycode;
    static int shift_pressed = 0;  /* Make it local static again */
    
    /* Simple scancode to ASCII for testing - only first 60 entries matter */
    static const char scancode_map[60] = {
        0, 27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
        '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
        0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
        0, '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,
        '*', 0, ' '
    };
    
    /* Shifted scancode map */
    static const char scancode_map_shift[60] = {
        0, 27, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
        '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
        0, 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~',
        0, '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0,
        '*', 0, ' '
    };
    
    while (1) {
        /* Wait for key event */
        do {
            status = inb(0x64);
        } while (!(status & 1));
        
        keycode = inb(0x60);
        
        /* Check for shift press/release (left shift = 0x2A, right shift = 0x36) */
        if (keycode == 0x2A || keycode == 0x36) {
            shift_pressed = 1;
            continue;  /* Get next key */
        } else if (keycode == 0xAA || keycode == 0xB6) {  /* Shift release */
            shift_pressed = 0;
            continue;  /* Get next key */
        }
        
        /* Skip key release events (high bit set) */
        if (keycode & 0x80) {
            continue;
        }
        
        /* Process key press */
        if (keycode < 60) {  /* Make sure we're within array bounds */
            char c;
            
            if (shift_pressed) {
                c = scancode_map_shift[(int)keycode];
            } else {
                c = scancode_map[(int)keycode];
            }
            
            /* Only return if we have a valid character */
            if (c != 0) {
                return c;
            }
        }
    }
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
