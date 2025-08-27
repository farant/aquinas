#include "display.h"
#include "vga.h"
#include "page.h"
#include "modes.h"
#include "rtc.h"
#include "serial.h"

/* Mouse state */
int mouse_x = 40;          /* Mouse X position (0-79) */
int mouse_y = 12;          /* Mouse Y position (0-24) */
int mouse_visible = 0;     /* Is mouse cursor visible */

/* Graphics mode flag (defined elsewhere, just declared extern here) */
extern int graphics_mode_active;

/* Draw navigation bar at top of screen */
void draw_nav_bar(void) {
    int i;
    unsigned short color;
    char page_info[40];
    char datetime_str[32];
    const char* mode_str;
    int mode_len = 0;
    int len = 0;
    int dt_len = 0;
    int page_num;
    int start_pos;
    rtc_time_t now;
    
    /* Don't draw nav bar when in graphics mode */
    if (graphics_mode_active) {
        return;
    }
    
    /* Fill top line with white background (inverse colors) */
    for (i = 0; i < VGA_WIDTH; i++) {
        color = VGA_COLOR_NAV_BAR;  /* Gray background, black text */
        /* Check if mouse is on this position */
        if (mouse_visible && mouse_y == 0 && mouse_x == i) {
            color = VGA_COLOR_MOUSE;  /* Green background for mouse cursor */
        }
        vga_write_char(i, ' ', color);
    }
    
    /* Display mode on the left side */
    mode_str = get_mode_string();
    /* Count mode string length */
    for (mode_len = 0; mode_str[mode_len]; mode_len++);
    
    /* Write mode with padding */
    for (i = 0; i < mode_len; i++) {
        color = editor_mode == MODE_INSERT ? 0x7200 : /* Green bg for insert */
                editor_mode == MODE_VISUAL ? 0x7400 : /* Red bg for visual */
                0x7800;  /* Yellow bg for normal */
        if (mouse_visible && mouse_y == 0 && mouse_x == i + 1) {
            color = VGA_COLOR_MOUSE;
        }
        vga_write_char(i + 1, mode_str[i], color);
    }
    
    /* Display page name if it exists */
    {
        Page *page = pages[current_page];
        int name_start = mode_len + 2;  /* Start after mode and a space */
        int name_len = 0;
        
        if (page && page->name[0] != '\0') {
            /* Count name length */
            while (page->name[name_len] && name_len < 63) {
                name_len++;
            }
            
            /* Write separator */
            vga_write_char(name_start - 1, ':', 0x7000);
            
            /* Write page name */
            for (i = 0; i < name_len; i++) {
                color = 0x7000;  /* Gray background */
                if (mouse_visible && mouse_y == 0 && mouse_x == name_start + i) {
                    color = VGA_COLOR_MOUSE;
                }
                vga_write_char(name_start + i, page->name[i], color);
            }
        }
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
    Page *page = pages[current_page];
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
    
    /* Don't draw text mode content when in graphics mode */
    if (graphics_mode_active) {
        return;
    }
    
    /* Clear only the text area (not nav bar) to prevent flickering */
    for (i = VGA_WIDTH; i < VGA_WIDTH * VGA_HEIGHT; i++) {
        vga_write_char(i, ' ', VGA_COLOR);
    }
    
    /* Always redraw navigation bar to update mouse cursor */
    draw_nav_bar();
    
    /* Get current page */
    page = pages[current_page];
    
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

/* Clear the screen */
void clear_screen(void) {
    Page *page;
    
    /* Use VGA module to clear screen */
    vga_clear_screen();
    
    /* Clear current page */
    page = pages[current_page];
    page->cursor_pos = 0;
    page->length = 0;
    update_cursor();
}