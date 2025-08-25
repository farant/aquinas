/* AquinasOS Text Editor 
 *
 * DESIGN
 * ------
 * 
 * This is a page-based text editor that runs as an operating system kernel.
 * The design emphasizes simplicity and independence between pages.
 * 
 * Architecture:
 * - Each page is completely independent with its own buffer and cursor
 * - Pages don't overflow into each other (no continuous buffer)
 * - Maximum of 100 pages, each holding one screen worth of text (24 lines)
 * - Navigation is done through a clickable navigation bar or keyboard shortcuts
 * 
 * Input handling:
 * - Non-blocking keyboard and mouse polling (no interrupts)
 * - Microsoft Serial Mouse protocol via COM1 (3-byte packets)
 * - Simultaneous mouse and keyboard input processing
 * 
 * Visual features:
 * - VGA text mode (80x25) with blue background
 * - Hardware cursor for text insertion point
 * - Green background for mouse cursor position
 * - Red background for highlighted text (click to select words)
 * - Auto-indentation when pressing Enter
 * 
 * The simplicity is intentional - this allows the editor to be reliable
 * and easy to understand, while still providing essential editing features.
 */

#include "io.h"
#include "serial.h"
#include "vga.h"
#include "timer.h"
#include "rtc.h"

/* Page size is one screen minus the navigation bar */
#define PAGE_SIZE ((VGA_HEIGHT - 1) * VGA_WIDTH)

/* Page structure - each page has its own buffer and cursor */
typedef struct {
    char buffer[((VGA_HEIGHT - 1) * VGA_WIDTH)];  /* One screen worth of text */
    int length;        /* Current length of text in this page */
    int cursor_pos;    /* Cursor position in this page */
    int highlight_start;  /* Start of highlighted text in this page */
    int highlight_end;    /* End of highlighted text in this page */
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

/* Forward declarations */
void refresh_screen(void);
void draw_nav_bar(void);
unsigned int get_stack_usage(void);

/* Port I/O functions now in io.h */

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
    int i;
    unsigned short color;
    char page_info[40];
    char datetime_str[32];
    int len = 0;
    int dt_len = 0;
    int page_num;
    int start_pos;
    rtc_time_t now;
    
    /* Fill top line with white background (inverse colors) */
    for (i = 0; i < VGA_WIDTH; i++) {
        color = VGA_COLOR_NAV_BAR;  /* Gray background, black text */
        /* Check if mouse is on this position */
        if (mouse_visible && mouse_y == 0 && mouse_x == i) {
            color = VGA_COLOR_MOUSE;  /* Green background for mouse cursor */
        }
        vga_write_char(i, ' ', color);
    }
    
    /* Format page info string */
    
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
    page_num = current_page + 1;
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
    start_pos = (VGA_WIDTH - len) / 2;
    for (i = 0; i < len; i++) {
        color = 0x7000;  /* Gray background */
        /* Check if mouse is on this position */
        if (mouse_visible && mouse_y == 0 && mouse_x == start_pos + i) {
            color = VGA_COLOR_MOUSE;  /* Green background for mouse cursor */
        }
        vga_write_char(start_pos + i, page_info[i], color);
    }
    
    /* Get current time for display in upper right */
    get_current_time(&now);
    
    /* Format date/time string: "MM/DD/YYYY HH:MM AM/PM" */
    /* Month - always 2 digits */
    datetime_str[dt_len++] = '0' + (now.month / 10);
    datetime_str[dt_len++] = '0' + (now.month % 10);
    datetime_str[dt_len++] = '/';
    
    /* Day - always 2 digits */
    datetime_str[dt_len++] = '0' + (now.day / 10);
    datetime_str[dt_len++] = '0' + (now.day % 10);
    datetime_str[dt_len++] = '/';
    
    /* Year - always 4 digits */
    datetime_str[dt_len++] = '0' + ((now.year / 1000) % 10);
    datetime_str[dt_len++] = '0' + ((now.year / 100) % 10);
    datetime_str[dt_len++] = '0' + ((now.year / 10) % 10);
    datetime_str[dt_len++] = '0' + (now.year % 10);
    datetime_str[dt_len++] = ' ';
    
    /* Convert to 12-hour format and determine AM/PM */
    {
        unsigned char display_hour = now.hour;
        int is_pm = 0;
        
        if (display_hour == 0) {
            display_hour = 12;  /* Midnight is 12 AM */
        } else if (display_hour == 12) {
            is_pm = 1;  /* Noon is 12 PM */
        } else if (display_hour > 12) {
            display_hour -= 12;
            is_pm = 1;
        }
        
        /* Hour - always 2 digits in 12-hour format */
        datetime_str[dt_len++] = '0' + (display_hour / 10);
        datetime_str[dt_len++] = '0' + (display_hour % 10);
        datetime_str[dt_len++] = ':';
        
        /* Minute - always 2 digits */
        datetime_str[dt_len++] = '0' + (now.minute / 10);
        datetime_str[dt_len++] = '0' + (now.minute % 10);
        datetime_str[dt_len++] = ' ';
        
        /* AM/PM */
        datetime_str[dt_len++] = is_pm ? 'P' : 'A';
        datetime_str[dt_len++] = 'M';
    }
    
    /* Display datetime in upper right corner (with 1 space padding from edge) */
    start_pos = VGA_WIDTH - dt_len - 1;
    for (i = 0; i < dt_len; i++) {
        color = 0x7F00;  /* Gray background, white text */
        /* Check if mouse is on this position */
        if (mouse_visible && mouse_y == 0 && mouse_x == start_pos + i) {
            color = VGA_COLOR_MOUSE;  /* Green background for mouse cursor */
        }
        vga_write_char(start_pos + i, datetime_str[i], color);
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
    
    /* Use VGA module to set cursor position */
    vga_set_cursor(screen_pos);
}

/* Redraw the screen from the buffer */
void refresh_screen(void) {
    int i;
    Page *page;
    int screen_pos;
    int buf_pos;
    unsigned short color;
    char c;
    int col;
    int j;
    unsigned short tab_color;
    
    /* Clear only the text area (not nav bar) to prevent flickering */
    for (i = VGA_WIDTH; i < VGA_WIDTH * VGA_HEIGHT; i++) {
        vga_write_char(i, ' ', VGA_COLOR);
    }
    
    /* Always redraw navigation bar to update mouse cursor */
    draw_nav_bar();
    
    /* Get current page */
    page = &pages[current_page];
    
    /* Start drawing text from line 1 (after nav bar) */
    screen_pos = VGA_WIDTH;  /* Skip first line */
    buf_pos = 0;
    
    while (screen_pos < VGA_WIDTH * VGA_HEIGHT && buf_pos < page->length) {
        color = VGA_COLOR;
        
        /* Check if this is the mouse position */
        if (mouse_visible && screen_pos == (mouse_y * VGA_WIDTH + mouse_x)) {
            color = VGA_COLOR_MOUSE;  /* Green background for mouse cursor */
        }
        
        /* Check if this position is highlighted (using per-page highlight) */
        if (page->highlight_end > 0 && page->highlight_end <= page->length &&
            page->highlight_start >= 0 && page->highlight_start < page->highlight_end &&
            buf_pos >= page->highlight_start && buf_pos < page->highlight_end) {
            color = VGA_COLOR_HIGHLIGHT;  /* Red background for highlighted text */
        }
        
        c = page->buffer[buf_pos];
        if (c == '\n') {
            /* Fill rest of line with spaces */
            col = screen_pos % VGA_WIDTH;
            while (col < VGA_WIDTH && screen_pos < VGA_WIDTH * VGA_HEIGHT) {
                /* Check mouse position for each space */
                if (mouse_visible && screen_pos == (mouse_y * VGA_WIDTH + mouse_x)) {
                    vga_write_char(screen_pos++, ' ', VGA_COLOR_MOUSE);
                } else {
                    vga_write_char(screen_pos++, ' ', VGA_COLOR);
                }
                col++;
            }
            buf_pos++;
        } else if (c == '\t') {
            /* Display tab as two spaces */
            for (j = 0; j < 2 && screen_pos < VGA_WIDTH * VGA_HEIGHT; j++) {
                tab_color = color;
                if (mouse_visible && screen_pos == (mouse_y * VGA_WIDTH + mouse_x)) {
                    tab_color = 0x2F00;
                }
                vga_write_char(screen_pos++, ' ', tab_color);
            }
            buf_pos++;
        } else {
            /* Regular character */
            vga_write_char(screen_pos++, c, color);
            buf_pos++;
        }
    }
    
    /* Fill remaining screen with spaces */
    while (screen_pos < VGA_WIDTH * VGA_HEIGHT) {
        if (mouse_visible && screen_pos == (mouse_y * VGA_WIDTH + mouse_x)) {
            vga_write_char(screen_pos++, ' ', VGA_COLOR_MOUSE);
        } else {
            vga_write_char(screen_pos++, ' ', VGA_COLOR);
        }
    }
    update_cursor();
}

/* Insert a character at cursor position */
void insert_char(char c) {
    Page *page = &pages[current_page];
    int line_start;
    int indent_count;
    int check_pos;
    int i;
    
    /* Check if page is full.
     * Why PAGE_SIZE - 1: We reserve one byte as a safety margin to prevent
     * buffer overflows during operations like auto-indentation which may
     * insert multiple characters. */
    if (page->length >= PAGE_SIZE - 1) return;
    
    /* If inserting newline, handle auto-indentation */
    if (c == '\n') {
        /* Find the start of the current line */
        line_start = page->cursor_pos;
        while (line_start > 0 && page->buffer[line_start - 1] != '\n') {
            line_start--;
        }
        
        /* Count leading spaces/tabs on current line */
        indent_count = 0;
        check_pos = line_start;
        while (check_pos < page->length && 
               (page->buffer[check_pos] == ' ' || page->buffer[check_pos] == '\t')) {
            indent_count++;
            check_pos++;
        }
        
        /* Make sure we have enough space for newline + indentation */
        if (page->length + 1 + indent_count >= PAGE_SIZE - 1) return;
        
        /* Shift everything after cursor forward to make room for newline + indentation */
        for (i = page->length + indent_count; i > page->cursor_pos; i--) {
            page->buffer[i] = page->buffer[i - 1 - indent_count];
        }
        
        /* Insert newline */
        page->buffer[page->cursor_pos] = '\n';
        page->cursor_pos++;
        page->length++;
        
        /* Copy indentation from current line */
        for (i = 0; i < indent_count; i++) {
            page->buffer[page->cursor_pos] = page->buffer[line_start + i];
            page->cursor_pos++;
            page->length++;
        }
    } else {
        /* Normal character insertion */
        /* Shift everything after cursor forward */
        for (i = page->length; i > page->cursor_pos; i--) {
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
    int i;
    
    if (page->cursor_pos == 0) return;
    
    /* Shift everything after cursor backward */
    for (i = page->cursor_pos - 1; i < page->length - 1; i++) {
        page->buffer[i] = page->buffer[i + 1];
    }
    
    page->cursor_pos--;
    page->length--;
    
    refresh_screen();
}

void clear_screen(void) {
    Page *page;
    
    /* Use VGA module to clear screen */
    vga_clear_screen();
    
    /* Clear current page */
    page = &pages[current_page];
    page->cursor_pos = 0;
    page->length = 0;
    update_cursor();
}

/* Port I/O functions now in io.h */

/* Serial port functions are now in serial.c */

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
    int line_start;
    int prev_line_start;
    int col;
    int prev_line_length;
    
    /* Find start of current line */
    line_start = page->cursor_pos;
    while (line_start > 0 && page->buffer[line_start - 1] != '\n') {
        line_start--;
    }
    
    /* If at first line, can't go up */
    if (line_start == 0) return;
    
    /* Find start of previous line */
    prev_line_start = line_start - 1;
    while (prev_line_start > 0 && page->buffer[prev_line_start - 1] != '\n') {
        prev_line_start--;
    }
    
    /* Calculate position in line */
    col = page->cursor_pos - line_start;
    
    /* Move to same column in previous line */
    prev_line_length = (line_start - 1) - prev_line_start;
    if (col > prev_line_length) {
        page->cursor_pos = line_start - 1; /* End of previous line */
    } else {
        page->cursor_pos = prev_line_start + col;
    }
    
    refresh_screen();
}

void move_cursor_down(void) {
    Page *page = &pages[current_page];
    int line_end;
    int line_start;
    int col;
    int next_line_start;
    int next_line_end;
    int next_line_length;
    
    /* Find end of current line */
    line_end = page->cursor_pos;
    while (line_end < page->length && page->buffer[line_end] != '\n') {
        line_end++;
    }
    
    /* If at last line, can't go down */
    if (line_end >= page->length) return;
    
    /* Find start of current line */
    line_start = page->cursor_pos;
    while (line_start > 0 && page->buffer[line_start - 1] != '\n') {
        line_start--;
    }
    
    /* Calculate position in line */
    col = page->cursor_pos - line_start;
    
    /* Find length of next line */
    next_line_start = line_end + 1;
    next_line_end = next_line_start;
    while (next_line_end < page->length && page->buffer[next_line_end] != '\n') {
        next_line_end++;
    }
    
    /* Move to same column in next line */
    next_line_length = next_line_end - next_line_start;
    if (col > next_line_length) {
        page->cursor_pos = next_line_end;
    } else {
        page->cursor_pos = next_line_start + col;
    }
    
    refresh_screen();
}

/* Old blocking keyboard handler - kept for reference but not used */
/* keyboard_get_scancode was replaced by non-blocking keyboard_check */

/* Poll for serial mouse data (non-blocking)
 *
 * MICROSOFT SERIAL MOUSE PROTOCOL
 * --------------------------------
 * 
 * Serial mice communicate using 3-byte packets at 1200 baud, 7N1.
 * The protocol was designed to be self-synchronizing - you can start
 * reading at any point and quickly identify packet boundaries.
 * 
 * Packet format:
 *   Byte 0: 01LR YYyy XXxx
 *           - Bit 6 is always 1 (identifies first byte)
 *           - Bit 5 (L) = left button state
 *           - Bit 4 (R) = right button state  
 *           - Bits 3-2 (YY) = Y movement high bits
 *           - Bits 1-0 (XX) = X movement high bits
 * 
 *   Byte 1: 00XX XXXX
 *           - Bit 6 is always 0 (not a first byte)
 *           - Bits 5-0 = X movement low 6 bits
 * 
 *   Byte 2: 00YY YYYY
 *           - Bit 6 is always 0 (not a first byte)
 *           - Bits 5-0 = Y movement low 6 bits
 * 
 * Movement values are 8-bit signed integers (-128 to +127).
 * Positive X = right, Positive Y = down (in mouse coordinates).
 * We invert Y since our screen coordinates have Y=0 at top.
 * 
 * The accumulator approach below smooths out small movements and
 * provides sub-pixel precision for better cursor control.
 */
void poll_mouse(void) {
    static unsigned char mouse_bytes[3];
    static int byte_num = 0;
    static int accumulated_dx = 0;  /* Accumulate fractional X movements */
    static int accumulated_dy = 0;  /* Accumulate fractional Y movements */
    static int prev_left_button = 0;  /* Track previous button state */
    unsigned char data;
    int packets_processed = 0;
    int left_button, right_button;
    int dx, dy;
    int old_x, old_y;
    int click_x, click_y;
    int nav_text_len, nav_start;
    
    /* Read all available bytes from serial port.
     * Why limit to 10 packets: We process at most 10 packets per poll to
     * prevent the mouse from monopolizing CPU time. Since we poll frequently,
     * this doesn't cause noticeable lag but ensures keyboard remains responsive. */
    while ((inb(COM1_LSR) & 0x01) && packets_processed < 10) {
        data = inb(COM1_DATA);
        
        /* Microsoft protocol: First byte has bit 6 set, bit 7 clear.
         * Why: This bit pattern allows re-synchronization if we start
         * reading in the middle of a packet stream. We can always find
         * the start of the next packet by looking for this signature. */
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
            
            /* Extract button states */
            left_button = (mouse_bytes[0] >> 5) & 1;
            right_button = (mouse_bytes[0] >> 4) & 1;
            
            /* Extract and combine movement values from split fields */
            dx = (mouse_bytes[1] & 0x3F) | ((mouse_bytes[0] & 0x03) << 6);
            dy = (mouse_bytes[2] & 0x3F) | ((mouse_bytes[0] & 0x0C) << 4);
            
            /* Sign extend from 8-bit to full int */
            if (dx & 0x80) dx = dx - 256;
            if (dy & 0x80) dy = dy - 256;
        
        /* Store old position to check if mouse moved */
        old_x = mouse_x;
        old_y = mouse_y;
        
        /* Check for left click BEFORE updating position */
        /* This uses the position where the button was pressed, not where mouse moved to */
        if (left_button && !prev_left_button) {
            /* Capture click at current position before any movement */
            click_x = mouse_x;
            click_y = mouse_y;
            
            /* Check if click is on navigation bar */
            if (click_y == 0) {
                /* Calculate where the buttons are - fixed positions now */
                nav_text_len = 27;  /* Fixed: 6 spaces/[prev] + space + "Page xx of yy [next]" */
                nav_start = (VGA_WIDTH - nav_text_len) / 2;
                
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
                /* Get current page */
                Page *page = &pages[current_page];
                
                int click_y = mouse_y - 1;
                if (click_y >= 0 && click_y < VGA_HEIGHT - 1) {
                    int click_x = mouse_x;
                    
                    /* Calculate buffer position from screen coordinates */
                    /* Simplified approach: directly calculate position */
                    int buf_pos = 0;
                    int line = 0;
                    int col = 0;
                    
                    /* Walk through the buffer character by character, tracking our
                     * screen position until we reach the clicked coordinates */
                    for (buf_pos = 0; buf_pos < page->length; buf_pos++) {
                        /* Check if we reached the clicked position */
                        if (line == click_y && col == click_x) {
                            break;
                        }
                        
                        /* Handle newlines */
                        if (page->buffer[buf_pos] == '\n') {
                            /* If we're on the target line but past the click column, we clicked past line end */
                            if (line == click_y) {
                                break;
                            }
                            line++;
                            col = 0;
                        } else if (page->buffer[buf_pos] == '\t') {
                            /* Tabs take up 2 visual spaces */
                            col += 2;
                            /* Handle line wrap */
                            if (col >= VGA_WIDTH) {
                                line++;
                                col = 0;
                            }
                        } else {
                            /* Normal character - move to next column */
                            col++;
                            /* Handle line wrap */
                            if (col >= VGA_WIDTH) {
                                line++;
                                col = 0;
                            }
                        }
                        
                        /* Stop if we've gone past the target line */
                        if (line > click_y) {
                            break;
                        }
                    }
                    
                    /* Check if click is within text */
                    if (buf_pos >= 0 && buf_pos < page->length) {
                        /* Check if clicked on a word or whitespace */
                        char clicked_char = page->buffer[buf_pos];
                        if (clicked_char == ' ' || clicked_char == '\n' || clicked_char == '\t') {
                            /* Clear any existing highlight */
                            page->highlight_start = 0;
                            page->highlight_end = 0;
                        } else {
                            /* Find and highlight the complete word */
                            page->highlight_start = buf_pos;
                            page->highlight_end = buf_pos + 1;  /* Start with at least one char */
                            
                            /* Find start of word */
                            while (page->highlight_start > 0 && 
                                   page->buffer[page->highlight_start - 1] != ' ' &&
                                   page->buffer[page->highlight_start - 1] != '\n' &&
                                   page->buffer[page->highlight_start - 1] != '\t') {
                                page->highlight_start--;
                            }
                            
                            /* Find end of word */
                            while (page->highlight_end < page->length &&
                                   page->buffer[page->highlight_end] != ' ' &&
                                   page->buffer[page->highlight_end] != '\n' &&
                                   page->buffer[page->highlight_end] != '\t') {
                                page->highlight_end++;
                            }
                            
                            /* Debug: Log the highlighted word (via COM2) */
                            serial_write_string("Highlighted word at position ");
                            serial_write_hex(buf_pos);
                            serial_write_string(" (");
                            serial_write_hex(page->highlight_start);
                            serial_write_string(" to ");
                            serial_write_hex(page->highlight_end);
                            serial_write_string(")\n");
                        }
                    } else {
                        /* Clicked beyond text - clear highlight */
                        page->highlight_start = 0;
                        page->highlight_end = 0;
                    }
                    
                    refresh_screen();
                }
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
    unsigned char status;
    unsigned char keycode;
    
    /* Check if keyboard data available (not mouse data) */
    status = inb(0x64);
    if (!(status & 1) || (status & 0x20)) {
        return 0;  /* No keyboard data available */
    }
    
    /* Read the keycode directly without blocking */
    keycode = inb(0x60);
    
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
/* Get current stack pointer value */
unsigned int get_esp(void) {
    unsigned int esp;
    __asm__ __volatile__("movl %%esp, %0" : "=r"(esp));
    return esp;
}

/* Calculate stack usage from initial 2MB position */
unsigned int get_stack_usage(void) {
    unsigned int current_esp = get_esp();
    unsigned int initial_esp = 0x200000;  /* Stack starts at 2MB */
    
    /* Stack grows downward, so usage is the difference */
    if (current_esp < initial_esp) {
        return initial_esp - current_esp;
    }
    return 0;  /* Stack pointer is above initial (shouldn't happen) */
}

/* Get maximum stack depth seen so far */
unsigned int get_max_stack_usage(void) {
    static unsigned int max_usage = 0;
    unsigned int current_usage = get_stack_usage();
    
    if (current_usage > max_usage) {
        max_usage = current_usage;
    }
    return max_usage;
}

void kernel_main(void) {
    int key;
    
    clear_screen();
    
    /* Initialize page management */
    current_page = 0;
    total_pages = 1;  /* Start with 1 page */
    
    /* Initialize first page */
    pages[0].length = 0;
    pages[0].cursor_pos = 0;
    
    /* DEBUG: Test if highlighting works at all */
    /* pages[0].highlight_start = 0;
    pages[0].highlight_end = 5; */
    
    /* Initialize debug serial port (COM2) */
    init_debug_serial();
    serial_write_string("\n\nAquinas OS started!\n");
    serial_write_string("COM2 debug port initialized.\n");
    
    /* Initialize timer system */
    init_timer();
    
    /* Initialize RTC to get current date/time */
    init_rtc();
    
    /* Initialize mouse (uses COM1) */
    init_mouse();
    serial_write_string("Mouse initialized on COM1.\n");
    serial_write_string("Text editor ready.\n");
    

    /* Start with empty screen, ready for typing */
    refresh_screen();

		serial_write_string("Made it past first refresh screen\n");
    
    /* Main editor loop - non-blocking */
    while (1) {
        /* Stack monitoring - report every 5 seconds using timer */
        static unsigned int last_stack_report = 0;
        static unsigned int last_clock_update = 0;
        unsigned int current_time = 0;


        current_time = get_ticks();

        
        if (get_elapsed_ms(last_stack_report) >= 5000) {
            unsigned int current_usage = get_stack_usage();
            unsigned int max_usage = get_max_stack_usage();
            rtc_time_t now;
            
            /* Get current time */
            get_current_time(&now);
            
            /* Print time and stack info */
            serial_write_string("[");
            if (now.hour < 10) serial_write_string("0");
            serial_write_int(now.hour);
            serial_write_string(":");
            if (now.minute < 10) serial_write_string("0");
            serial_write_int(now.minute);
            serial_write_string(":");
            if (now.second < 10) serial_write_string("0");
            serial_write_int(now.second);
            serial_write_string("] Stack: ");
            serial_write_int(current_usage);
            serial_write_string("/");
            serial_write_int(max_usage);
            serial_write_string(" bytes, ESP=");
            serial_write_hex(get_esp());
            serial_write_string("\n");
            
            last_stack_report = current_time;
        }
        
        /* Update clock display every second */
        if (get_elapsed_ms(last_clock_update) >= 1000) {
            draw_nav_bar();  /* Redraw nav bar to update time */
            last_clock_update = current_time;
        }
        
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
