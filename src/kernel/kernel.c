/* AquinasOS Text Editor */

#define VGA_BUFFER ((unsigned short*)0xB8000)
#define VGA_WIDTH 80
#define VGA_HEIGHT 25
#define VGA_COLOR 0x1F00  /* Blue background, white text */
#define BUFFER_SIZE (VGA_WIDTH * VGA_HEIGHT * 4)  /* 4 screens worth */

/* Text buffer to store document */
char text_buffer[BUFFER_SIZE];
int buffer_length = 0;     /* Current length of text in buffer */
int cursor_pos = 0;        /* Cursor position in the buffer */
int screen_offset = 0;     /* First character shown on screen */

/* Write a byte to port */
static inline void outb(unsigned short port, unsigned char val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "dN"(port));
}

/* Update hardware cursor position */
void update_cursor(void) {
    /* Calculate visual position accounting for newlines and tabs */
    int screen_pos = 0;
    int buf_pos = screen_offset;
    
    while (buf_pos < cursor_pos && screen_pos < VGA_WIDTH * VGA_HEIGHT) {
        if (buf_pos < buffer_length) {
            char c = text_buffer[buf_pos];
            if (c == '\n') {
                /* Jump to next line */
                int col = screen_pos % VGA_WIDTH;
                screen_pos += (VGA_WIDTH - col);
            } else if (c == '\t') {
                /* Tab takes 2 screen positions */
                screen_pos += 2;
            } else {
                screen_pos++;
            }
        } else {
            screen_pos++;
        }
        buf_pos++;
    }
    
    if (screen_pos >= 0 && screen_pos < VGA_WIDTH * VGA_HEIGHT) {
        /* Tell VGA board we're setting the high cursor byte */
        outb(0x3D4, 0x0E);
        outb(0x3D5, (unsigned char)((screen_pos >> 8) & 0xFF));
        
        /* Tell VGA board we're setting the low cursor byte */
        outb(0x3D4, 0x0F);
        outb(0x3D5, (unsigned char)(screen_pos & 0xFF));
    }
}

/* Redraw the screen from the buffer */
void refresh_screen(void) {
    int screen_pos = 0;
    int buf_pos = screen_offset;
    
    while (screen_pos < VGA_WIDTH * VGA_HEIGHT) {
        if (buf_pos < buffer_length) {
            char c = text_buffer[buf_pos];
            if (c == '\n') {
                /* Fill rest of line with spaces */
                int col = screen_pos % VGA_WIDTH;
                while (col < VGA_WIDTH && screen_pos < VGA_WIDTH * VGA_HEIGHT) {
                    VGA_BUFFER[screen_pos++] = VGA_COLOR | ' ';
                    col++;
                }
                buf_pos++;
            } else if (c == '\t') {
                /* Display tab as two spaces */
                if (screen_pos < VGA_WIDTH * VGA_HEIGHT) {
                    VGA_BUFFER[screen_pos++] = VGA_COLOR | ' ';
                }
                if (screen_pos < VGA_WIDTH * VGA_HEIGHT) {
                    VGA_BUFFER[screen_pos++] = VGA_COLOR | ' ';
                }
                buf_pos++;
            } else {
                /* Regular character */
                VGA_BUFFER[screen_pos++] = VGA_COLOR | c;
                buf_pos++;
            }
        } else {
            /* Past end of buffer, fill with spaces */
            VGA_BUFFER[screen_pos++] = VGA_COLOR | ' ';
        }
    }
    update_cursor();
}

/* Insert a character at cursor position */
void insert_char(char c) {
    if (buffer_length >= BUFFER_SIZE - 1) return; /* Buffer full */
    
    /* Shift everything after cursor forward */
    for (int i = buffer_length; i > cursor_pos; i--) {
        text_buffer[i] = text_buffer[i - 1];
    }
    
    /* Insert the character */
    text_buffer[cursor_pos] = c;
    cursor_pos++;
    buffer_length++;
    
    refresh_screen();
}

/* Delete character before cursor (backspace) */
void delete_char(void) {
    if (cursor_pos == 0) return;
    
    /* Shift everything after cursor backward */
    for (int i = cursor_pos - 1; i < buffer_length - 1; i++) {
        text_buffer[i] = text_buffer[i + 1];
    }
    
    cursor_pos--;
    buffer_length--;
    
    refresh_screen();
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

/* Move cursor */
void move_cursor_left(void) {
    if (cursor_pos > 0) {
        cursor_pos--;
        refresh_screen();
    }
}

void move_cursor_right(void) {
    if (cursor_pos < buffer_length) {
        cursor_pos++;
        refresh_screen();
    }
}

void move_cursor_up(void) {
    /* Find start of current line */
    int line_start = cursor_pos;
    while (line_start > 0 && text_buffer[line_start - 1] != '\n') {
        line_start--;
    }
    
    /* If at first line, can't go up */
    if (line_start == 0) return;
    
    /* Find start of previous line */
    int prev_line_start = line_start - 1;
    while (prev_line_start > 0 && text_buffer[prev_line_start - 1] != '\n') {
        prev_line_start--;
    }
    
    /* Calculate position in line */
    int col = cursor_pos - line_start;
    
    /* Move to same column in previous line */
    int prev_line_length = (line_start - 1) - prev_line_start;
    if (col > prev_line_length) {
        cursor_pos = line_start - 1; /* End of previous line */
    } else {
        cursor_pos = prev_line_start + col;
    }
    
    refresh_screen();
}

void move_cursor_down(void) {
    /* Find end of current line */
    int line_end = cursor_pos;
    while (line_end < buffer_length && text_buffer[line_end] != '\n') {
        line_end++;
    }
    
    /* If at last line, can't go down */
    if (line_end >= buffer_length) return;
    
    /* Find start of current line */
    int line_start = cursor_pos;
    while (line_start > 0 && text_buffer[line_start - 1] != '\n') {
        line_start--;
    }
    
    /* Calculate position in line */
    int col = cursor_pos - line_start;
    
    /* Find length of next line */
    int next_line_start = line_end + 1;
    int next_line_end = next_line_start;
    while (next_line_end < buffer_length && text_buffer[next_line_end] != '\n') {
        next_line_end++;
    }
    
    /* Move to same column in next line */
    int next_line_length = next_line_end - next_line_start;
    if (col > next_line_length) {
        cursor_pos = next_line_end;
    } else {
        cursor_pos = next_line_start + col;
    }
    
    refresh_screen();
}

/* Keyboard handler with special keys */
int keyboard_get_scancode(void) {
    unsigned char status;
    unsigned char keycode;
    static int shift_pressed = 0;
    
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
        
        /* Special keys - return negative values */
        /* Arrow keys: Up=0x48, Left=0x4B, Right=0x4D, Down=0x50 */
        if (keycode == 0x48) return -1;  /* Up arrow */
        if (keycode == 0x50) return -2;  /* Down arrow */
        if (keycode == 0x4B) return -3;  /* Left arrow */
        if (keycode == 0x4D) return -4;  /* Right arrow */
        
        /* Process regular key press */
        if (keycode < 60) {  /* Make sure we're within array bounds */
            char c;
            
            if (shift_pressed) {
                c = scancode_map_shift[(int)keycode];
            } else {
                c = scancode_map[(int)keycode];
            }
            
            /* Return character (positive value) */
            if (c != 0) {
                return (int)c;
            }
        }
    }
}

/* Kernel entry point */
void kernel_main(void) {
    int key;
    
    clear_screen();
    
    /* Initialize empty buffer */
    buffer_length = 0;
    cursor_pos = 0;
    screen_offset = 0;
    
    /* Start with empty screen, ready for typing */
    refresh_screen();
    
    /* Main editor loop */
    while (1) {
        key = keyboard_get_scancode();
        
        if (key == -1) {  /* Up arrow */
            move_cursor_up();
        } else if (key == -2) {  /* Down arrow */
            move_cursor_down();
        } else if (key == -3) {  /* Left arrow */
            move_cursor_left();
        } else if (key == -4) {  /* Right arrow */
            move_cursor_right();
        } else if (key == '\b') {  /* Backspace */
            delete_char();
        } else if (key == '\t') {  /* Tab - insert actual tab character */
            insert_char('\t');
        } else if (key > 0 && key != 27) {  /* Regular character, ignore ESC */
            insert_char((char)key);
        }
    }
}
