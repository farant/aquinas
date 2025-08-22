/* kernel.c - Simple x86_64 kernel */

/* VGA text mode buffer */
#define VGA_BUFFER 0xB8000
#define VGA_WIDTH 80
#define VGA_HEIGHT 25
#define VGA_COLOR_WHITE 0x0F

typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef unsigned long uint64_t;

static uint16_t* const vga_buffer = (uint16_t*)VGA_BUFFER;
static int cursor_x = 0;
static int cursor_y = 0;

/* Output a character to screen */
void putchar(char c) {
    if (c == '\n') {
        cursor_x = 0;
        cursor_y++;
        if (cursor_y >= VGA_HEIGHT) {
            /* Scroll screen */
            int y, x;
            for (y = 0; y < VGA_HEIGHT - 1; y++) {
                for (x = 0; x < VGA_WIDTH; x++) {
                    vga_buffer[y * VGA_WIDTH + x] = vga_buffer[(y + 1) * VGA_WIDTH + x];
                }
            }
            for (x = 0; x < VGA_WIDTH; x++) {
                vga_buffer[(VGA_HEIGHT - 1) * VGA_WIDTH + x] = (VGA_COLOR_WHITE << 8) | ' ';
            }
            cursor_y = VGA_HEIGHT - 1;
        }
        return;
    }
    
    vga_buffer[cursor_y * VGA_WIDTH + cursor_x] = (VGA_COLOR_WHITE << 8) | c;
    cursor_x++;
    
    if (cursor_x >= VGA_WIDTH) {
        cursor_x = 0;
        cursor_y++;
    }
}

/* Print a string */
void puts(const char* str) {
    while (*str) {
        putchar(*str++);
    }
}

/* Clear screen */
void clear_screen(void) {
    int i;
    for (i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) {
        vga_buffer[i] = (VGA_COLOR_WHITE << 8) | ' ';
    }
    cursor_x = 0;
    cursor_y = 0;
}

/* Simple memory copy */
void* memcpy(void* dest, const void* src, uint64_t n) {
    uint8_t* d = (uint8_t*)dest;
    const uint8_t* s = (const uint8_t*)src;
    while (n--) {
        *d++ = *s++;
    }
    return dest;
}

/* Simple memory set */
void* memset(void* dest, int val, uint64_t n) {
    uint8_t* d = (uint8_t*)dest;
    while (n--) {
        *d++ = (uint8_t)val;
    }
    return dest;
}

/* Convert integer to string */
void itoa(uint64_t value, char* str, int base) {
    char* ptr = str;
    char* ptr1 = str;
    char tmp_char;
    uint64_t tmp_value;
    
    if (base < 2 || base > 36) {
        *str = '\0';
        return;
    }
    
    do {
        tmp_value = value;
        value /= base;
        *ptr++ = "0123456789abcdefghijklmnopqrstuvwxyz"[tmp_value - value * base];
    } while (value);
    
    *ptr-- = '\0';
    
    while (ptr1 < ptr) {
        tmp_char = *ptr;
        *ptr-- = *ptr1;
        *ptr1++ = tmp_char;
    }
}

/* Print hexadecimal number */
void print_hex(uint64_t value) {
    char buffer[20];
    puts("0x");
    itoa(value, buffer, 16);
    puts(buffer);
}

/* Simple keyboard input (polling) */
static uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

/* Check if keyboard has data */
int keyboard_has_data(void) {
    return inb(0x64) & 1;
}

/* Read keyboard scancode */
uint8_t keyboard_read_scancode(void) {
    while (!keyboard_has_data());
    return inb(0x60);
}

/* Simple command interpreter */
void process_command(char* cmd) {
    if (cmd[0] == 'h' && cmd[1] == 'e' && cmd[2] == 'l' && cmd[3] == 'p' && cmd[4] == '\0') {
        puts("Available commands:\n");
        puts("  help    - Show this help\n");
        puts("  clear   - Clear screen\n");
        puts("  mem     - Show memory info\n");
        puts("  echo    - Echo text\n");
    } else if (cmd[0] == 'c' && cmd[1] == 'l' && cmd[2] == 'e' && cmd[3] == 'a' && cmd[4] == 'r' && cmd[5] == '\0') {
        clear_screen();
    } else if (cmd[0] == 'm' && cmd[1] == 'e' && cmd[2] == 'm' && cmd[3] == '\0') {
        puts("Memory map:\n");
        puts("  0x00000000 - 0x00000FFF : Real mode IVT\n");
        puts("  0x00001000 - 0x00004FFF : Page tables\n");
        puts("  0x00007C00 - 0x00007DFF : Boot sector\n");
        puts("  0x00008000 - 0x0000FFFF : Kernel\n");
        puts("  0x000B8000 - 0x000BFFFF : VGA buffer\n");
    } else if (cmd[0] == 'e' && cmd[1] == 'c' && cmd[2] == 'h' && cmd[3] == 'o' && cmd[4] == ' ') {
        puts(&cmd[5]);
        puts("\n");
    } else if (cmd[0] != '\0') {
        puts("Unknown command: ");
        puts(cmd);
        puts("\n");
    }
}

/* Kernel entry point */
void kernel_main(void) {
    char cmd_buffer[256];
    int cmd_pos = 0;
    uint8_t scancode;
    
    /* Quick test - write directly to VGA to confirm we're running */
    volatile uint16_t* test = (uint16_t*)0xB8000;
    *test = 0x0F00 | 'K';  /* White 'K' in top-left corner */
    
    clear_screen();
    puts("64-bit OS Kernel Started!\n");
    puts("Type 'help' for available commands\n\n");
    
    /* Main loop */
    while (1) {
        puts("> ");
        cmd_pos = 0;
        
        /* Read command */
        while (1) {
            scancode = keyboard_read_scancode();
            
            /* Simple scancode to ASCII mapping */
            if (scancode == 0x1C) { /* Enter */
                cmd_buffer[cmd_pos] = '\0';
                putchar('\n');
                break;
            } else if (scancode == 0x0E) { /* Backspace */
                if (cmd_pos > 0) {
                    cmd_pos--;
                    cursor_x--;
                    putchar(' ');
                    cursor_x--;
                }
            } else if (scancode < 0x80) { /* Key press (not release) */
                /* Simple mapping for letters and space */
                char c = 0;
                switch(scancode) {
                    case 0x10: c = 'q'; break;
                    case 0x11: c = 'w'; break;
                    case 0x12: c = 'e'; break;
                    case 0x13: c = 'r'; break;
                    case 0x14: c = 't'; break;
                    case 0x15: c = 'y'; break;
                    case 0x16: c = 'u'; break;
                    case 0x17: c = 'i'; break;
                    case 0x18: c = 'o'; break;
                    case 0x19: c = 'p'; break;
                    case 0x1E: c = 'a'; break;
                    case 0x1F: c = 's'; break;
                    case 0x20: c = 'd'; break;
                    case 0x21: c = 'f'; break;
                    case 0x22: c = 'g'; break;
                    case 0x23: c = 'h'; break;
                    case 0x24: c = 'j'; break;
                    case 0x25: c = 'k'; break;
                    case 0x26: c = 'l'; break;
                    case 0x2C: c = 'z'; break;
                    case 0x2D: c = 'x'; break;
                    case 0x2E: c = 'c'; break;
                    case 0x2F: c = 'v'; break;
                    case 0x30: c = 'b'; break;
                    case 0x31: c = 'n'; break;
                    case 0x32: c = 'm'; break;
                    case 0x39: c = ' '; break;
                }
                
                if (c && cmd_pos < 255) {
                    cmd_buffer[cmd_pos++] = c;
                    putchar(c);
                }
            }
        }
        
        process_command(cmd_buffer);
    }
}
