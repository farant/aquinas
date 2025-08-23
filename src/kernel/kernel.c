/* AquinasOS Text Editor */

#define VGA_BUFFER ((unsigned short*)0xB8000)
#define VGA_WIDTH 80
#define VGA_HEIGHT 25
#define VGA_COLOR 0x1F00  /* Blue background, white text */
#define BUFFER_SIZE (VGA_WIDTH * VGA_HEIGHT * 4)  /* 4 screens worth */

/* Page structure - each page has its own buffer and cursor */
typedef struct {
    char buffer[((VGA_HEIGHT - 1) * VGA_WIDTH)];  /* One screen worth of text */
    int length;        /* Current length of text in this page */
    int cursor_pos;    /* Cursor position in this page */
} Page;

/* Page management */
#define PAGE_SIZE ((VGA_HEIGHT - 1) * VGA_WIDTH)  /* Characters per page */
#define MAX_PAGES 100
Page pages[MAX_PAGES];
int current_page = 0;
int total_pages = 1;

/* Mouse state */
int mouse_x = 40;          /* Mouse X position (0-79) */
int mouse_y = 12;          /* Mouse Y position (0-24) */
int mouse_visible = 0;     /* Is mouse cursor visible */
int highlight_start = -1;  /* Start of highlighted word */
int highlight_end = -1;    /* End of highlighted word */

/* Forward declarations */
void refresh_screen(void);
void draw_nav_bar(void);

/* Write a byte to port */
static inline void outb(unsigned short port, unsigned char val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "dN"(port));
}

/* Switch to previous page */
void prev_page(void) {
    if (current_page > 0) {
        current_page--;
        /* Cursor position is already stored in the page structure */
        refresh_screen();
    }
}

/* Switch to next page */
void next_page(void) {
    if (current_page < MAX_PAGES - 1) {
        current_page++;
        /* Update total pages if we're creating a new page */
        if (current_page >= total_pages) {
            total_pages = current_page + 1;
            /* Initialize new page */
            pages[current_page].length = 0;
            pages[current_page].cursor_pos = 0;
        }
        /* Cursor position is already stored in the page structure */
        refresh_screen();
    }
}

/* Draw navigation bar at top of screen */
void draw_nav_bar(void) {
    /* Fill top line with white background (inverse colors) */
    for (int i = 0; i < VGA_WIDTH; i++) {
        unsigned short color = 0x7000;  /* Gray background, black text */
        /* Check if mouse is on this position */
        if (mouse_visible && mouse_y == 0 && mouse_x == i) {
            color = 0x2F00;  /* Green background for mouse cursor */
        }
        VGA_BUFFER[i] = color | ' ';
    }
    
    /* Format page info string */
    char page_info[40];
    int len = 0;
    
    /* Always reserve space for [prev], but only draw if not on first page */
    if (current_page > 0) {
        page_info[len++] = '[';
        page_info[len++] = 'p';
        page_info[len++] = 'r';
        page_info[len++] = 'e';
        page_info[len++] = 'v';
        page_info[len++] = ']';
    } else {
        /* Add spaces to keep alignment */
        page_info[len++] = ' ';
        page_info[len++] = ' ';
        page_info[len++] = ' ';
        page_info[len++] = ' ';
        page_info[len++] = ' ';
        page_info[len++] = ' ';
    }
    page_info[len++] = ' ';
    
    /* Add "Page n of x" */
    page_info[len++] = 'P';
    page_info[len++] = 'a';
    page_info[len++] = 'g';
    page_info[len++] = 'e';
    page_info[len++] = ' ';
    
    /* Add current page number */
    int page_num = current_page + 1;
    if (page_num >= 10) {
        page_info[len++] = '0' + (page_num / 10);
    }
    page_info[len++] = '0' + (page_num % 10);
    
    page_info[len++] = ' ';
    page_info[len++] = 'o';
    page_info[len++] = 'f';
    page_info[len++] = ' ';
    
    /* Add total pages */
    if (total_pages >= 10) {
        page_info[len++] = '0' + (total_pages / 10);
    }
    page_info[len++] = '0' + (total_pages % 10);
    
    page_info[len++] = ' ';
    page_info[len++] = '[';
    page_info[len++] = 'n';
    page_info[len++] = 'e';
    page_info[len++] = 'x';
    page_info[len++] = 't';
    page_info[len++] = ']';
    
    /* Center the text in the nav bar */
    int start_pos = (VGA_WIDTH - len) / 2;
    for (int i = 0; i < len; i++) {
        unsigned short color = 0x7000;  /* Gray background */
        /* Check if mouse is on this position */
        if (mouse_visible && mouse_y == 0 && mouse_x == start_pos + i) {
            color = 0x2F00;  /* Green background for mouse cursor */
        }
        VGA_BUFFER[start_pos + i] = color | page_info[i];
    }
}

/* Update hardware cursor position */
void update_cursor(void) {
    /* Calculate visual position accounting for newlines and tabs */
    Page *page = &pages[current_page];
    int screen_pos = VGA_WIDTH;  /* Start after nav bar */
    int buf_pos = 0;
    
    while (buf_pos < page->cursor_pos && screen_pos < VGA_WIDTH * VGA_HEIGHT) {
        if (buf_pos < page->length) {
            char c = page->buffer[buf_pos];
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
    /* Clear only the text area (not nav bar) to prevent flickering */
    for (int i = VGA_WIDTH; i < VGA_WIDTH * VGA_HEIGHT; i++) {
        VGA_BUFFER[i] = VGA_COLOR | ' ';
    }
    
    /* Always redraw navigation bar to update mouse cursor */
    draw_nav_bar();
    
    /* Get current page */
    Page *page = &pages[current_page];
    
    /* Start drawing text from line 1 (after nav bar) */
    int screen_pos = VGA_WIDTH;  /* Skip first line */
    int buf_pos = 0;
    
    while (screen_pos < VGA_WIDTH * VGA_HEIGHT && buf_pos < page->length) {
        unsigned short color = VGA_COLOR;
        
        /* Check if this is the mouse position */
        if (mouse_visible && screen_pos == (mouse_y * VGA_WIDTH + mouse_x)) {
            color = 0x2F00;  /* Green background for mouse cursor */
        }
        
        /* Check if this position is highlighted */
        if (highlight_start >= 0 && buf_pos >= highlight_start && buf_pos < highlight_end) {
            color = 0x4F00;  /* Red background for highlighted text */
        }
        
        char c = page->buffer[buf_pos];
        if (c == '\n') {
            /* Fill rest of line with spaces */
            int col = screen_pos % VGA_WIDTH;
            while (col < VGA_WIDTH && screen_pos < VGA_WIDTH * VGA_HEIGHT) {
                /* Check mouse position for each space */
                if (mouse_visible && screen_pos == (mouse_y * VGA_WIDTH + mouse_x)) {
                    VGA_BUFFER[screen_pos++] = 0x2F00 | ' ';
                } else {
                    VGA_BUFFER[screen_pos++] = VGA_COLOR | ' ';
                }
                col++;
            }
            buf_pos++;
        } else if (c == '\t') {
            /* Display tab as two spaces */
            for (int i = 0; i < 2 && screen_pos < VGA_WIDTH * VGA_HEIGHT; i++) {
                unsigned short tab_color = color;
                if (mouse_visible && screen_pos == (mouse_y * VGA_WIDTH + mouse_x)) {
                    tab_color = 0x2F00;
                }
                VGA_BUFFER[screen_pos++] = tab_color | ' ';
            }
            buf_pos++;
        } else {
            /* Regular character */
            VGA_BUFFER[screen_pos++] = color | c;
            buf_pos++;
        }
    }
    
    /* Fill remaining screen with spaces */
    while (screen_pos < VGA_WIDTH * VGA_HEIGHT) {
        if (mouse_visible && screen_pos == (mouse_y * VGA_WIDTH + mouse_x)) {
            VGA_BUFFER[screen_pos++] = 0x2F00 | ' ';
        } else {
            VGA_BUFFER[screen_pos++] = VGA_COLOR | ' ';
        }
    }
    update_cursor();
}

/* Insert a character at cursor position */
void insert_char(char c) {
    Page *page = &pages[current_page];
    
    /* Check if page is full */
    if (page->length >= PAGE_SIZE - 1) return;
    
    /* If inserting newline, handle auto-indentation */
    if (c == '\n') {
        /* Find the start of the current line */
        int line_start = page->cursor_pos;
        while (line_start > 0 && page->buffer[line_start - 1] != '\n') {
            line_start--;
        }
        
        /* Count leading spaces/tabs on current line */
        int indent_count = 0;
        int check_pos = line_start;
        while (check_pos < page->length && 
               (page->buffer[check_pos] == ' ' || page->buffer[check_pos] == '\t')) {
            indent_count++;
            check_pos++;
        }
        
        /* Make sure we have enough space for newline + indentation */
        if (page->length + 1 + indent_count >= PAGE_SIZE - 1) return;
        
        /* Shift everything after cursor forward to make room for newline + indentation */
        for (int i = page->length + indent_count; i > page->cursor_pos; i--) {
            page->buffer[i] = page->buffer[i - 1 - indent_count];
        }
        
        /* Insert newline */
        page->buffer[page->cursor_pos] = '\n';
        page->cursor_pos++;
        page->length++;
        
        /* Copy indentation from current line */
        for (int i = 0; i < indent_count; i++) {
            page->buffer[page->cursor_pos] = page->buffer[line_start + i];
            page->cursor_pos++;
            page->length++;
        }
    } else {
        /* Normal character insertion */
        /* Shift everything after cursor forward */
        for (int i = page->length; i > page->cursor_pos; i--) {
            page->buffer[i] = page->buffer[i - 1];
        }
        
        /* Insert the character */
        page->buffer[page->cursor_pos] = c;
        page->cursor_pos++;
        page->length++;
    }
    
    refresh_screen();
}

/* Delete character before cursor (backspace) */
void delete_char(void) {
    Page *page = &pages[current_page];
    
    if (page->cursor_pos == 0) return;
    
    /* Shift everything after cursor backward */
    for (int i = page->cursor_pos - 1; i < page->length - 1; i++) {
        page->buffer[i] = page->buffer[i + 1];
    }
    
    page->cursor_pos--;
    page->length--;
    
    refresh_screen();
}

void clear_screen(void) {
    for (int i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) {
        VGA_BUFFER[i] = VGA_COLOR | ' ';
    }
    /* Clear current page */
    Page *page = &pages[current_page];
    page->cursor_pos = 0;
    page->length = 0;
    update_cursor();
}

/* Read a byte from port */
static inline unsigned char inb(unsigned short port) {
    unsigned char ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "dN"(port));
    return ret;
}

/* Serial port definitions */
#define COM1 0x3F8
#define COM1_DATA (COM1 + 0)
#define COM1_IER  (COM1 + 1)  /* Interrupt Enable Register */
#define COM1_FCR  (COM1 + 2)  /* FIFO Control Register */
#define COM1_LCR  (COM1 + 3)  /* Line Control Register */
#define COM1_MCR  (COM1 + 4)  /* Modem Control Register */
#define COM1_LSR  (COM1 + 5)  /* Line Status Register */

/* Initialize serial port for mouse */
void init_serial_port(void) {
    /* Disable interrupts */
    outb(COM1_IER, 0x00);
    
    /* Set baud rate to 1200 (divisor = 96) for serial mouse */
    outb(COM1_LCR, 0x80);  /* Enable DLAB */
    outb(COM1_DATA, 0x60); /* Set divisor low byte (96) */
    outb(COM1_IER, 0x00);  /* Set divisor high byte (0) */
    
    /* 7 data bits, 1 stop bit, no parity (Microsoft mouse protocol) */
    outb(COM1_LCR, 0x02);
    
    /* Enable FIFO */
    outb(COM1_FCR, 0xC7);
    
    /* Enable DTR/RTS to power the mouse */
    outb(COM1_MCR, 0x03);
}

/* Initialize serial mouse */
void init_mouse(void) {
    init_serial_port();
    
    /* Send 'M' to identify as Microsoft mouse */
    /* Some mice auto-detect, others need this */
    while (inb(COM1_LSR) & 0x01) {
        inb(COM1_DATA);  /* Clear any pending data */
    }
    
    mouse_visible = 1;
}

/* Move cursor */
void move_cursor_left(void) {
    Page *page = &pages[current_page];
    if (page->cursor_pos > 0) {
        page->cursor_pos--;
        refresh_screen();
    }
}

void move_cursor_right(void) {
    Page *page = &pages[current_page];
    if (page->cursor_pos < page->length) {
        page->cursor_pos++;
        refresh_screen();
    }
}

void move_cursor_up(void) {
    Page *page = &pages[current_page];
    
    /* Find start of current line */
    int line_start = page->cursor_pos;
    while (line_start > 0 && page->buffer[line_start - 1] != '\n') {
        line_start--;
    }
    
    /* If at first line, can't go up */
    if (line_start == 0) return;
    
    /* Find start of previous line */
    int prev_line_start = line_start - 1;
    while (prev_line_start > 0 && page->buffer[prev_line_start - 1] != '\n') {
        prev_line_start--;
    }
    
    /* Calculate position in line */
    int col = page->cursor_pos - line_start;
    
    /* Move to same column in previous line */
    int prev_line_length = (line_start - 1) - prev_line_start;
    if (col > prev_line_length) {
        page->cursor_pos = line_start - 1; /* End of previous line */
    } else {
        page->cursor_pos = prev_line_start + col;
    }
    
    refresh_screen();
}

void move_cursor_down(void) {
    Page *page = &pages[current_page];
    
    /* Find end of current line */
    int line_end = page->cursor_pos;
    while (line_end < page->length && page->buffer[line_end] != '\n') {
        line_end++;
    }
    
    /* If at last line, can't go down */
    if (line_end >= page->length) return;
    
    /* Find start of current line */
    int line_start = page->cursor_pos;
    while (line_start > 0 && page->buffer[line_start - 1] != '\n') {
        line_start--;
    }
    
    /* Calculate position in line */
    int col = page->cursor_pos - line_start;
    
    /* Find length of next line */
    int next_line_start = line_end + 1;
    int next_line_end = next_line_start;
    while (next_line_end < page->length && page->buffer[next_line_end] != '\n') {
        next_line_end++;
    }
    
    /* Move to same column in next line */
    int next_line_length = next_line_end - next_line_start;
    if (col > next_line_length) {
        page->cursor_pos = next_line_end;
    } else {
        page->cursor_pos = next_line_start + col;
    }
    
    refresh_screen();
}

/* Old blocking keyboard handler - kept for reference but not used */
/* keyboard_get_scancode was replaced by non-blocking keyboard_check */

/* Poll for serial mouse data (non-blocking) */
void poll_mouse(void) {
    static unsigned char mouse_bytes[3];
    static int byte_num = 0;
    static int accumulated_dx = 0;  /* Accumulate fractional X movements */
    static int accumulated_dy = 0;  /* Accumulate fractional Y movements */
    static int prev_left_button = 0;  /* Track previous button state */
    unsigned char data;
    int packets_processed = 0;
    
    /* Read all available bytes from serial port */
    while ((inb(COM1_LSR) & 0x01) && packets_processed < 10) {
        data = inb(COM1_DATA);
        
        /* Microsoft protocol: First byte has bit 6 set, bit 7 clear */
        /* Also validate that subsequent bytes don't have bit 6 set */
        if ((data & 0xC0) == 0x40) {
            /* This looks like a first byte - start new packet */
            byte_num = 0;
            mouse_bytes[0] = data;
            byte_num = 1;
        } else if (byte_num > 0 && (data & 0x40) == 0) {
            /* This looks like a continuation byte (bit 6 clear) */
            mouse_bytes[byte_num++] = data;
        } else {
            /* Invalid byte - reset and wait for valid first byte */
            byte_num = 0;
            continue;
        }
        
        /* Process complete packet */
        if (byte_num == 3) {
            byte_num = 0;  /* Reset for next packet */
            packets_processed++;
            
            /* Parse Microsoft mouse packet */
            /* Byte 0: 01LR YYyy XXxx (L=left button, R=right button) */
            /* Byte 1: 00XX XXXX (X movement, 6 bits) */
            /* Byte 2: 00YY YYYY (Y movement, 6 bits) */
            
            int left_button = (mouse_bytes[0] >> 5) & 1;
            int right_button = (mouse_bytes[0] >> 4) & 1;
            
            /* Extract 8-bit signed movement values */
            /* Combine high bits from byte 0 with low bits from bytes 1&2 */
            int dx = (mouse_bytes[1] & 0x3F) | ((mouse_bytes[0] & 0x03) << 6);
            int dy = (mouse_bytes[2] & 0x3F) | ((mouse_bytes[0] & 0x0C) << 4);
            
            /* Sign extend from 8-bit to full int */
            if (dx & 0x80) dx = dx - 256;
            if (dy & 0x80) dy = dy - 256;
        
        /* Store old position to check if mouse moved */
        int old_x = mouse_x;
        int old_y = mouse_y;
        
        /* Check for left click BEFORE updating position */
        /* This uses the position where the button was pressed, not where mouse moved to */
        if (left_button && !prev_left_button) {
            /* Capture click at current position before any movement */
            int click_x = mouse_x;
            int click_y = mouse_y;
            
            /* Check if click is on navigation bar */
            if (click_y == 0) {
                /* Calculate where the buttons are - fixed positions now */
                int nav_text_len = 27;  /* Fixed: 6 spaces/[prev] + space + "Page xx of yy [next]" */
                int nav_start = (VGA_WIDTH - nav_text_len) / 2;
                
                /* Check for [prev] button click (only if visible) */
                if (current_page > 0 && click_x >= nav_start && click_x < nav_start + 6) {
                    prev_page();
                }
                /* Check for [next] button click - always at same position */
                else if (click_x >= nav_start + nav_text_len - 6 && click_x < nav_start + nav_text_len) {
                    next_page();
                }
            }
            /* Normal text click handling */
            else {
                /* Word highlighting disabled for now with new page structure */
                /* Clear any existing highlight */
                highlight_start = -1;
                highlight_end = -1;
                
                /* TODO: Reimplement click-to-position cursor and word highlighting */
                
                refresh_screen();
            }  /* End of else (normal text click) */
        }
        
        /* Only update mouse position if button is NOT held down */
        /* This prevents cursor movement during clicks/drags */
        if (!left_button) {
            /* Accumulate mouse movements for smooth, responsive control */
            /* This ensures even tiny movements eventually register */
            accumulated_dx += dx;
            accumulated_dy += dy;
            
            /* Apply accumulated movement with scaling */
            /* Divide by 10 for slower, more controlled movement */
            if (accumulated_dx != 0) {
                /* Add threshold for horizontal movement to prevent column jitter */
                /* Less strict than vertical to keep horizontal movement fluid */
                int move_x = accumulated_dx / 12;  /* Slightly higher divisor for X axis */
                if (move_x == 0 && (accumulated_dx > 10 || accumulated_dx < -10)) {
                    /* Only move if accumulated enough (more than 10 units) */
                    /* This prevents accidental column changes */
                    move_x = (accumulated_dx > 0) ? 1 : -1;
                    accumulated_dx -= move_x * 12;  /* Consume the movement */
                } else if (move_x != 0) {
                    accumulated_dx -= move_x * 12;  /* Keep remainder for next time */
                }
                /* If not enough accumulation, keep building up */
                mouse_x += move_x;
            }
            
            if (accumulated_dy != 0) {
                /* Add extra threshold for vertical movement to prevent line jitter */
                /* Require more accumulation before moving vertically */
                int move_y = accumulated_dy / 15;  /* Higher divisor for Y axis */
                if (move_y == 0 && (accumulated_dy > 12 || accumulated_dy < -12)) {
                    /* Only move if accumulated enough (more than 12 units) */
                    /* This prevents accidental line changes */
                    move_y = (accumulated_dy > 0) ? 1 : -1;
                    accumulated_dy -= move_y * 15;  /* Consume the movement */
                } else if (move_y != 0) {
                    accumulated_dy -= move_y * 15;  /* Keep remainder for next time */
                }
                /* If not enough accumulation, keep building up */
                mouse_y += move_y;
            }
            
            /* Clamp to screen bounds and clear accumulated values at edges */
            if (mouse_x < 0) {
                mouse_x = 0;
                accumulated_dx = 0;  /* Clear accumulator at edge */
            }
            if (mouse_x >= VGA_WIDTH) {
                mouse_x = VGA_WIDTH - 1;
                accumulated_dx = 0;  /* Clear accumulator at edge */
            }
            if (mouse_y < 0) {
                mouse_y = 0;
                accumulated_dy = 0;  /* Clear accumulator at edge */
            }
            if (mouse_y >= VGA_HEIGHT) {
                mouse_y = VGA_HEIGHT - 1;
                accumulated_dy = 0;  /* Clear accumulator at edge */
            }
        } else {
            /* While button is held, discard movement data */
            /* This prevents accumulation during drag */
            accumulated_dx = 0;
            accumulated_dy = 0;
        }
        
        /* Refresh if mouse moved */
        if (mouse_x != old_x || mouse_y != old_y) {
            refresh_screen();
        }
        
        /* Update previous button state for next frame */
        prev_left_button = left_button;
        
        }  /* End of if (byte_num == 3) */
    }  /* End of while loop reading serial data */
}

/* Non-blocking keyboard check */
/* Shift key state - must persist between calls */
static int shift_pressed = 0;

int keyboard_check(void) {
    unsigned char status;
    unsigned char keycode;
    
    /* Check if keyboard data available (not mouse data) */
    status = inb(0x64);
    if (!(status & 1) || (status & 0x20)) {
        return 0;  /* No keyboard data available */
    }
    
    /* Read the keycode directly without blocking */
    keycode = inb(0x60);
    
    /* Simple scancode to ASCII - extended to handle all keys properly */
    static const char scancode_map[128] = {
        0, 27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',  /* 0-14 */
        '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',    /* 15-28 */
        0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',            /* 29-41 */
        0, '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,              /* 42-54 */
        '*', 0, ' ', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,                       /* 55-70 */
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,                           /* 71-86 */
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,                           /* 87-102 */
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,                           /* 103-118 */
        0, 0, 0, 0, 0, 0, 0, 0, 0                                                  /* 119-127 */
    };
    
    /* Shifted scancode map */
    static const char scancode_map_shift[128] = {
        0, 27, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',  /* 0-14 */
        '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',    /* 15-28 */
        0, 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~',            /* 29-41 */
        0, '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0,              /* 42-54 */
        '*', 0, ' ', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,                       /* 55-70 */
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,                           /* 71-86 */
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,                           /* 87-102 */
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,                           /* 103-118 */
        0, 0, 0, 0, 0, 0, 0, 0, 0                                                  /* 119-127 */
    };
    
    /* Check for shift press/release (left shift = 0x2A, right shift = 0x36) */
    if (keycode == 0x2A || keycode == 0x36) {
        shift_pressed = 1;
        return 0;  /* Don't return anything for shift press */
    } else if (keycode == 0xAA || keycode == 0xB6) {  /* Shift release */
        shift_pressed = 0;
        return 0;
    }
    
    /* Skip key release events (high bit set) */
    if (keycode & 0x80) {
        return 0;
    }
    
    /* Special keys - return negative values */
    /* Arrow keys: Up=0x48, Left=0x4B, Right=0x4D, Down=0x50 */
    if (keycode == 0x48) return -1;  /* Up arrow */
    if (keycode == 0x50) return -2;  /* Down arrow */
    if (keycode == 0x4B) {
        if (shift_pressed) return -5;  /* Shift+Left = Previous page */
        return -3;  /* Left arrow */
    }
    if (keycode == 0x4D) {
        if (shift_pressed) return -6;  /* Shift+Right = Next page */
        return -4;  /* Right arrow */
    }
    
    /* Process regular key press */
    if (keycode < 128) {  /* Make sure we're within array bounds */
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
    
    return 0;
}

/* Kernel entry point */
void kernel_main(void) {
    int key;
    
    clear_screen();
    
    /* Initialize page management */
    current_page = 0;
    total_pages = 1;  /* Start with 1 page */
    
    /* Initialize first page */
    pages[0].length = 0;
    pages[0].cursor_pos = 0;
    
    /* Initialize mouse */
    init_mouse();
    
    /* Start with empty screen, ready for typing */
    refresh_screen();
    
    /* Main editor loop - non-blocking */
    while (1) {
        /* Poll for mouse data (will refresh screen if mouse moves) */
        poll_mouse();
        
        /* Check for keyboard input (non-blocking) */
        key = keyboard_check();
        
        if (key == -1) {  /* Up arrow */
            move_cursor_up();
        } else if (key == -2) {  /* Down arrow */
            move_cursor_down();
        } else if (key == -3) {  /* Left arrow */
            move_cursor_left();
        } else if (key == -4) {  /* Right arrow */
            move_cursor_right();
        } else if (key == -5) {  /* Shift+Left = Previous page */
            prev_page();
        } else if (key == -6) {  /* Shift+Right = Next page */
            next_page();
        } else if (key == '\b') {  /* Backspace */
            delete_char();
        } else if (key == '\t') {  /* Tab - insert actual tab character */
            insert_char('\t');
        } else if (key > 0 && key != 27) {  /* Regular character, ignore ESC */
            insert_char((char)key);
        }
    }
}
