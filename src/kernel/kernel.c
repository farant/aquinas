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
#include "memory.h"
#include "graphics.h"
#include "dispi.h"
#include "display_driver.h"
#include "font_6x8.h"
#include "dispi_cursor.h"

/* From graphics.c - VGA state functions */
void save_vga_font(void);
void restore_vga_font(void);
void restore_dac_palette(void);

/* Page size is one screen minus the navigation bar */
#define PAGE_SIZE ((VGA_HEIGHT - 1) * VGA_WIDTH)

/* Page structure - each page has its own buffer and cursor */
typedef struct {
    char* buffer;      /* Dynamically allocated buffer */
    int length;        /* Current length of text in this page */
    int cursor_pos;    /* Cursor position in this page */
    int highlight_start;  /* Start of highlighted text in this page */
    int highlight_end;    /* End of highlighted text in this page */
    char name[64];     /* Optional page name (empty string if unnamed) */
} Page;

/* Page management */
#define PAGE_SIZE ((VGA_HEIGHT - 1) * VGA_WIDTH)  /* Characters per page */
#define MAX_PAGES 100
Page* pages[MAX_PAGES];  /* Array of pointers to pages */
int current_page = 0;
int total_pages = 1;

/* Navigation history for #back functionality */
#define HISTORY_SIZE 32
int page_history[HISTORY_SIZE];
int history_pos = 0;
int history_count = 0;

/* Editor modes */
typedef enum {
    MODE_NORMAL,    /* Normal/command mode (vim-like) */
    MODE_INSERT,    /* Insert mode for typing */
    MODE_VISUAL     /* Visual mode for selection */
} EditorMode;

EditorMode editor_mode = MODE_INSERT;  /* Start in insert mode for now */

/* 'fd' escape sequence timeout in milliseconds 
 * When 'f' is typed, it's inserted immediately. If 'd' follows within
 * this timeout window, the 'f' is deleted and editor exits to normal mode.
 * This avoids delays when typing multiple 'f's in succession.
 */
#define FD_ESCAPE_TIMEOUT_MS 300

/* Mouse state */
int mouse_x = 40;          /* Mouse X position (0-79) */
int mouse_y = 12;          /* Mouse Y position (0-24) */
int mouse_visible = 0;     /* Is mouse cursor visible */

/* Forward declarations */
void refresh_screen(void);
void draw_nav_bar(void);
void set_mode(EditorMode mode);
const char* get_mode_string(void);
void execute_command(Page* page, int cmd_start, int cmd_end);
void navigate_to_page(int new_page);
void execute_link(Page* page, int link_start, int link_end);
void test_dispi_driver(void);
static void draw_dispi_circle(int cx, int cy, int r, unsigned char color);

/* Allocate a new page */
Page* allocate_page(void) {
    Page* page = (Page*)malloc(sizeof(Page));
    if (page == NULL) {
        serial_write_string("ERROR: Failed to allocate page structure\n");
        return NULL;
    }
    
    /* Allocate buffer for the page */
    page->buffer = (char*)calloc(PAGE_SIZE, sizeof(char));
    if (page->buffer == NULL) {
        serial_write_string("ERROR: Failed to allocate page buffer\n");
        /* Note: We can't free the page in bump allocator, but that's ok */
        return NULL;
    }
    
    /* Initialize page fields */
    page->length = 0;
    page->cursor_pos = 0;
    page->highlight_start = 0;
    page->highlight_end = 0;
    page->name[0] = '\0';  /* Empty name initially */
    
    return page;
}

/* Initialize page array */
void init_pages(void) {
    int i;
    
    /* Clear all page pointers */
    for (i = 0; i < MAX_PAGES; i++) {
        pages[i] = NULL;
    }
    
    /* Allocate the first page */
    pages[0] = allocate_page();
    if (pages[0] == NULL) {
        /* Critical error - can't continue without at least one page */
        serial_write_string("FATAL: Could not allocate initial page\n");
        /* In a real OS, we'd panic here */
    }
    
    current_page = 0;
    total_pages = 1;
}
unsigned int get_stack_usage(void);

/* Port I/O functions now in io.h */

/* Mode management functions */
void set_mode(EditorMode mode) {
    editor_mode = mode;
    /* Refresh to show new mode in status */
    draw_nav_bar();
}

const char* get_mode_string(void) {
    switch (editor_mode) {
        case MODE_NORMAL:
            return "NORMAL";
        case MODE_INSERT:
            return "INSERT";
        case MODE_VISUAL:
            return "VISUAL";
        default:
            return "UNKNOWN";
    }
}

/* Navigate to a specific page with history tracking */
void navigate_to_page(int new_page) {
    if (new_page == current_page) return;  /* Already on this page */
    
    if (new_page < 0) new_page = 0;
    if (new_page >= MAX_PAGES) new_page = MAX_PAGES - 1;
    
    /* Record current page in history before navigating away */
    if (history_count < HISTORY_SIZE) {
        page_history[history_count] = current_page;
        history_count++;
        history_pos = history_count;  /* Reset position to end */
    } else {
        /* History is full, shift everything left and add new entry */
        int i;
        for (i = 0; i < HISTORY_SIZE - 1; i++) {
            page_history[i] = page_history[i + 1];
        }
        page_history[HISTORY_SIZE - 1] = current_page;
        /* history_pos stays at HISTORY_SIZE */
    }
    
    /* Navigate to the new page */
    current_page = new_page;
    
    /* Create page if it doesn't exist yet */
    if (current_page >= total_pages) {
        total_pages = current_page + 1;
        /* Allocate and initialize new page */
        pages[current_page] = allocate_page();
        if (pages[current_page] == NULL) {
            serial_write_string("ERROR: Failed to allocate new page\n");
            /* Go back to previous page */
            if (history_count > 0) {
                current_page = page_history[history_count - 1];
                history_count--;  /* Remove the failed navigation from history */
            }
            return;
        }
    }
    
    refresh_screen();
}

/* Switch to previous page */
void prev_page(void) {
    if (current_page > 0) {
        navigate_to_page(current_page - 1);
    }
}

/* Switch to next page */
void next_page(void) {
    navigate_to_page(current_page + 1);
}

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

/* Execute a command that starts with $ */
void execute_command(Page* page, int cmd_start, int cmd_end) {
    char cmd_name[32];
    int cmd_len;
    int i;
    int insert_pos;
    char output[64];
    int output_len;
    rtc_time_t now;
    int space_after;
    int visual_space_count;
    int scan_pos;
    int col;
    
    /* Extract command name */
    cmd_len = cmd_end - cmd_start;
    if (cmd_len >= 32) cmd_len = 31;
    
    for (i = 0; i < cmd_len; i++) {
        cmd_name[i] = page->buffer[cmd_start + i];
    }
    cmd_name[cmd_len] = '\0';
    
    /* Debug output */
    serial_write_string("Executing command: ");
    for (i = 0; i < cmd_len; i++) {
        serial_write_char(cmd_name[i]);
    }
    serial_write_char('\n');
    
    /* Process commands */
    if (cmd_len == 5 && cmd_name[1] == 'd' && cmd_name[2] == 'a' && 
        cmd_name[3] == 't' && cmd_name[4] == 'e') {
        /* $date command - insert current date/time */
        get_current_time(&now);
        
        /* Format date as MM/DD/YYYY HH:MM */
        output_len = 0;
        
        /* Month */
        if (now.month >= 10) {
            output[output_len++] = '0' + (now.month / 10);
        } else {
            output[output_len++] = '0';
        }
        output[output_len++] = '0' + (now.month % 10);
        output[output_len++] = '/';
        
        /* Day */
        if (now.day >= 10) {
            output[output_len++] = '0' + (now.day / 10);
        } else {
            output[output_len++] = '0';
        }
        output[output_len++] = '0' + (now.day % 10);
        output[output_len++] = '/';
        
        /* Year */
        output[output_len++] = '0' + ((now.year / 1000) % 10);
        output[output_len++] = '0' + ((now.year / 100) % 10);
        output[output_len++] = '0' + ((now.year / 10) % 10);
        output[output_len++] = '0' + (now.year % 10);
        output[output_len++] = ' ';
        
        /* Hour */
        if (now.hour >= 10) {
            output[output_len++] = '0' + (now.hour / 10);
        } else {
            output[output_len++] = '0';
        }
        output[output_len++] = '0' + (now.hour % 10);
        output[output_len++] = ':';
        
        /* Minute */
        if (now.minute >= 10) {
            output[output_len++] = '0' + (now.minute / 10);
        } else {
            output[output_len++] = '0';
        }
        output[output_len++] = '0' + (now.minute % 10);
        
        /* Determine insertion position */
        insert_pos = cmd_end;
        
        /* Check if there's already a space after the command */
        space_after = 0;
        if (insert_pos < page->length && page->buffer[insert_pos] == ' ') {
            space_after = 1;
            insert_pos++;  /* Skip the existing space */
        }
        
        /* Count visual whitespace after command position */
        visual_space_count = 0;
        scan_pos = insert_pos;
        col = 0;
        
        /* Calculate column position of insert_pos */
        for (i = 0; i < insert_pos && i < page->length; i++) {
            if (page->buffer[i] == '\n') {
                col = 0;
            } else if (page->buffer[i] == '\t') {
                col += 2;
            } else {
                col++;
            }
        }
        
        /* Count spaces until we hit non-whitespace or newline */
        while (scan_pos < page->length && visual_space_count < output_len) {
            if (page->buffer[scan_pos] == ' ') {
                visual_space_count++;
                scan_pos++;
                col++;
            } else if (page->buffer[scan_pos] == '\n') {
                /* Count remaining columns in the line as available space */
                while (col < VGA_WIDTH && visual_space_count < output_len) {
                    visual_space_count++;
                    col++;
                }
                break;
            } else {
                /* Hit non-whitespace character */
                break;
            }
        }
        
        /* Check if we have enough room */
        if (page->length + output_len + 1 - visual_space_count >= PAGE_SIZE) {
            serial_write_string("Not enough space for command output\n");
            return;
        }
        
        /* Add space to output to separate from following text */
        output[output_len++] = ' ';
        
        /* If we have enough visual space, overwrite it */
        if (visual_space_count >= output_len) {
            /* Just overwrite the spaces */
            for (i = 0; i < output_len; i++) {
                page->buffer[insert_pos + i] = output[i];
            }
        } else {
            /* Need to make room - shift text right */
            int shift_amount = output_len - visual_space_count;
            
            /* Add space before output if not already there */
            if (!space_after) {
                shift_amount++;  /* Need one more byte for the space */
            }
            
            /* Shift existing text to make room */
            for (i = page->length - 1; i >= insert_pos + visual_space_count; i--) {
                page->buffer[i + shift_amount] = page->buffer[i];
            }
            
            /* Insert space if needed */
            if (!space_after) {
                page->buffer[cmd_end] = ' ';
                insert_pos = cmd_end + 1;
            }
            
            /* Insert the output */
            for (i = 0; i < output_len; i++) {
                page->buffer[insert_pos + i] = output[i];
            }
            
            /* Update page length */
            page->length += shift_amount;
        }
        
        /* Clear highlight after command execution */
        page->highlight_start = 0;
        page->highlight_end = 0;
        
        /* Refresh display */
        refresh_screen();
    } else if (cmd_len == 7 && cmd_name[1] == 'r' && cmd_name[2] == 'e' && 
               cmd_name[3] == 'n' && cmd_name[4] == 'a' && cmd_name[5] == 'm' && 
               cmd_name[6] == 'e') {
        /* $rename command - look for name after the command */
        int name_start = cmd_end;
        int name_end = cmd_end;
        int name_len = 0;
        int j;
        
        /* Skip any spaces after $rename */
        while (name_start < page->length && page->buffer[name_start] == ' ') {
            name_start++;
        }
        
        /* Find the end of the name (next space or newline) */
        name_end = name_start;
        while (name_end < page->length && 
               page->buffer[name_end] != ' ' && 
               page->buffer[name_end] != '\n' &&
               page->buffer[name_end] != '\t') {
            name_end++;
        }
        
        /* Extract the new name */
        if (name_start < name_end) {
            /* We have a name argument */
            name_len = name_end - name_start;
            if (name_len > 63) name_len = 63;  /* Limit to 63 chars */
            
            /* Copy the name */
            for (j = 0; j < name_len; j++) {
                page->name[j] = page->buffer[name_start + j];
            }
            page->name[name_len] = '\0';
            
            serial_write_string("Page renamed to: ");
            for (j = 0; j < name_len; j++) {
                serial_write_char(page->name[j]);
            }
            serial_write_char('\n');
        } else {
            /* No name provided - clear the name */
            page->name[0] = '\0';
            serial_write_string("Page name cleared\n");
        }
        
        /* Clear highlight after command execution */
        page->highlight_start = 0;
        page->highlight_end = 0;
        
        /* Refresh display to show new name in nav bar */
        refresh_screen();
    }
    else if (cmd_len == 9 && cmd_name[1] == 'g' && cmd_name[2] == 'r' &&
             cmd_name[3] == 'a' && cmd_name[4] == 'p' && cmd_name[5] == 'h' &&
             cmd_name[6] == 'i' && cmd_name[7] == 'c' && cmd_name[8] == 's') {
        /* $graphics command - switch to graphics mode for demo */
        serial_write_string("Entering graphics mode demo\n");
        
        /* Run the graphics demo (will return when ESC is pressed) */
        graphics_demo();
        
        /* Screen needs to be redrawn after returning from graphics mode */
        refresh_screen();
        
        /* Clear highlight after command execution */
        page->highlight_start = 0;
        page->highlight_end = 0;
    }
    else if (cmd_len == 6 && cmd_name[1] == 'd' && cmd_name[2] == 'i' &&
             cmd_name[3] == 's' && cmd_name[4] == 'p' && cmd_name[5] == 'i') {
        /* $dispi command - test DISPI driver */
        serial_write_string("Testing DISPI driver\n");
        
        /* Test the DISPI driver */
        test_dispi_driver();
        
        /* Screen needs to be redrawn after returning from graphics mode */
        refresh_screen();
        
        /* Clear highlight after command execution */
        page->highlight_start = 0;
        page->highlight_end = 0;
    }
}

/* Execute a link that starts with # */
void execute_link(Page* page, int link_start, int link_end) {
    char link_text[64];
    int link_len;
    int i;
    int target_page = -1;
    
    /* Extract link text (skip the #) */
    link_len = link_end - link_start - 1;  /* -1 to skip # */
    if (link_len >= 64) link_len = 63;
    
    for (i = 0; i < link_len; i++) {
        link_text[i] = page->buffer[link_start + 1 + i];  /* +1 to skip # */
    }
    link_text[link_len] = '\0';
    
    /* Debug output */
    serial_write_string("Clicking link: #");
    for (i = 0; i < link_len; i++) {
        serial_write_char(link_text[i]);
    }
    serial_write_char('\n');
    
    /* Check for #back */
    if (link_len == 4 && link_text[0] == 'b' && link_text[1] == 'a' && 
        link_text[2] == 'c' && link_text[3] == 'k') {
        /* Go back in history */
        if (history_count > 0) {
            int prev_page = page_history[history_count - 1];
            history_count--;  /* Remove from history */
            current_page = prev_page;
            refresh_screen();
        }
        /* Clear highlight */
        page->highlight_start = 0;
        page->highlight_end = 0;
        return;
    }
    
    /* Check for #last-page */
    if (link_len == 9 && link_text[0] == 'l' && link_text[1] == 'a' && 
        link_text[2] == 's' && link_text[3] == 't' && link_text[4] == '-' &&
        link_text[5] == 'p' && link_text[6] == 'a' && link_text[7] == 'g' && 
        link_text[8] == 'e') {
        target_page = total_pages - 1;
    }
    /* Check if it's a page number */
    else if (link_len > 0 && link_text[0] >= '0' && link_text[0] <= '9') {
        /* Parse page number */
        target_page = 0;
        for (i = 0; i < link_len; i++) {
            if (link_text[i] >= '0' && link_text[i] <= '9') {
                target_page = target_page * 10 + (link_text[i] - '0');
            } else {
                /* Invalid number */
                target_page = -1;
                break;
            }
        }
        if (target_page > 0) {
            target_page--;  /* Convert to 0-based index */
        }
    }
    /* Otherwise, it's a page name - search for it */
    else if (link_len > 0) {
        int p;
        for (p = 0; p < total_pages; p++) {
            if (pages[p] && pages[p]->name[0] != '\0') {
                /* Compare page name with link text */
                int matches = 1;
                for (i = 0; i < link_len; i++) {
                    if (pages[p]->name[i] != link_text[i]) {
                        matches = 0;
                        break;
                    }
                }
                /* Check that name ends here too */
                if (matches && pages[p]->name[link_len] == '\0') {
                    target_page = p;
                    break;
                }
            }
        }
    }
    
    /* Navigate if we found a valid target */
    if (target_page >= 0) {
        navigate_to_page(target_page);
    } else {
        serial_write_string("Link target not found\n");
    }
    
    /* Clear highlight */
    page->highlight_start = 0;
    page->highlight_end = 0;
}

/* Insert a character at cursor position */
void insert_char(char c) {
    Page *page = pages[current_page];
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
    Page *page = pages[current_page];
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
    page = pages[current_page];
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
    Page *page = pages[current_page];
    if (page->cursor_pos > 0) {
        page->cursor_pos--;
        refresh_screen();
    }
}

void move_cursor_right(void) {
    Page *page = pages[current_page];
    if (page->cursor_pos < page->length) {
        page->cursor_pos++;
        refresh_screen();
    }
}

void move_cursor_up(void) {
    Page *page = pages[current_page];
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
    Page *page = pages[current_page];
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

/* Delete current line */
void delete_line(void) {
    Page *page = pages[current_page];
    int line_start, line_end;
    int delete_count;
    int i;
    
    /* Find start of current line */
    line_start = page->cursor_pos;
    while (line_start > 0 && page->buffer[line_start - 1] != '\n') {
        line_start--;
    }
    
    /* Find end of current line (including newline) */
    line_end = line_start;
    while (line_end < page->length && page->buffer[line_end] != '\n') {
        line_end++;
    }
    if (line_end < page->length && page->buffer[line_end] == '\n') {
        line_end++;  /* Include the newline */
    }
    
    /* Calculate how many characters to delete */
    delete_count = line_end - line_start;
    
    /* Shift remaining text left */
    for (i = line_end; i < page->length; i++) {
        page->buffer[i - delete_count] = page->buffer[i];
    }
    
    /* Update length and cursor */
    page->length -= delete_count;
    page->cursor_pos = line_start;
    
    /* Move to first non-space character of next line if exists */
    while (page->cursor_pos < page->length && 
           (page->buffer[page->cursor_pos] == ' ' || 
            page->buffer[page->cursor_pos] == '\t')) {
        page->cursor_pos++;
    }
    
    refresh_screen();
}

/* Delete to end of line */
void delete_to_eol(void) {
    Page *page = pages[current_page];
    int line_end;
    int delete_count;
    int i;
    
    /* Find end of current line (not including newline) */
    line_end = page->cursor_pos;
    while (line_end < page->length && page->buffer[line_end] != '\n') {
        line_end++;
    }
    
    /* Calculate how many characters to delete */
    delete_count = line_end - page->cursor_pos;
    
    if (delete_count > 0) {
        /* Shift remaining text left */
        for (i = line_end; i < page->length; i++) {
            page->buffer[i - delete_count] = page->buffer[i];
        }
        
        /* Update length */
        page->length -= delete_count;
        
        refresh_screen();
    }
}

/* Delete from cursor to beginning of line's first non-whitespace (vim d^ command) */
void delete_to_bol(void) {
    Page *page = pages[current_page];
    int line_start;
    int first_non_ws;
    int delete_start;
    int delete_count;
    int i;
    
    /* Find start of current line */
    line_start = page->cursor_pos;
    while (line_start > 0 && page->buffer[line_start - 1] != '\n') {
        line_start--;
    }
    
    /* Find first non-whitespace character position */
    first_non_ws = line_start;
    while (first_non_ws < page->length && 
           first_non_ws < page->cursor_pos &&
           (page->buffer[first_non_ws] == ' ' || 
            page->buffer[first_non_ws] == '\t')) {
        first_non_ws++;
    }
    
    /* Delete from first_non_ws to cursor_pos (not including cursor_pos) */
    delete_start = first_non_ws;
    delete_count = page->cursor_pos - delete_start;
    
    if (delete_count > 0) {
        /* Shift remaining text left */
        for (i = page->cursor_pos; i < page->length; i++) {
            page->buffer[i - delete_count] = page->buffer[i];
        }
        
        /* Update length and cursor position */
        page->length -= delete_count;
        page->cursor_pos = delete_start;
        
        refresh_screen();
    }
}

/* Delete till character */
void delete_till_char(char target) {
    Page *page = pages[current_page];
    int end_pos;
    int delete_count;
    int i;
    
    /* Find target character */
    end_pos = page->cursor_pos;
    while (end_pos < page->length && 
           page->buffer[end_pos] != target && 
           page->buffer[end_pos] != '\n') {
        end_pos++;
    }
    
    /* Don't delete if we hit newline or end of buffer instead of target */
    if (end_pos >= page->length || page->buffer[end_pos] != target) {
        return;
    }
    
    /* Calculate how many characters to delete (not including target) */
    delete_count = end_pos - page->cursor_pos;
    
    if (delete_count > 0) {
        /* Shift remaining text left */
        for (i = end_pos; i < page->length; i++) {
            page->buffer[i - delete_count] = page->buffer[i];
        }
        
        /* Update length */
        page->length -= delete_count;
        
        refresh_screen();
    }
}

/* Insert a new line below current line and enter insert mode (vim 'o' command) */
void insert_line_below(void) {
    Page *page = pages[current_page];
    int line_end;
    int line_start;
    int indent_count;
    int check_pos;
    int i;
    
    /* Find end of current line */
    line_end = page->cursor_pos;
    while (line_end < page->length && page->buffer[line_end] != '\n') {
        line_end++;
    }
    
    /* Find start of current line to get indentation */
    line_start = page->cursor_pos;
    while (line_start > 0 && page->buffer[line_start - 1] != '\n') {
        line_start--;
    }
    
    /* Count leading spaces/tabs on current line for auto-indent */
    indent_count = 0;
    check_pos = line_start;
    while (check_pos < page->length && 
           (page->buffer[check_pos] == ' ' || page->buffer[check_pos] == '\t')) {
        indent_count++;
        check_pos++;
    }
    
    /* Check if we have enough space for newline + indentation */
    if (page->length + 1 + indent_count >= PAGE_SIZE - 1) return;
    
    /* Move cursor to end of current line */
    page->cursor_pos = line_end;
    
    /* Shift everything after line_end forward to make room */
    for (i = page->length + indent_count; i > line_end; i--) {
        page->buffer[i] = page->buffer[i - 1 - indent_count];
    }
    
    /* Insert newline */
    page->buffer[line_end] = '\n';
    page->cursor_pos = line_end + 1;
    page->length++;
    
    /* Copy indentation from current line (preserving tabs/spaces) */
    for (i = 0; i < indent_count; i++) {
        page->buffer[page->cursor_pos] = page->buffer[line_start + i];
        page->cursor_pos++;
        page->length++;
    }
    
    /* Enter insert mode */
    set_mode(MODE_INSERT);
    refresh_screen();
}

/* Insert a new line above current line and enter insert mode (vim 'O' command) */
void insert_line_above(void) {
    Page *page = pages[current_page];
    int line_start;
    int original_line_start;
    int indent_count;
    int check_pos;
    int i;
    char indent_chars[80];  /* Store indentation characters */
    
    /* Find start of current line */
    line_start = page->cursor_pos;
    while (line_start > 0 && page->buffer[line_start - 1] != '\n') {
        line_start--;
    }
    original_line_start = line_start;
    
    /* Count and save indentation from current line */
    indent_count = 0;
    check_pos = line_start;
    while (check_pos < page->length && 
           (page->buffer[check_pos] == ' ' || page->buffer[check_pos] == '\t') &&
           indent_count < 80) {
        indent_chars[indent_count] = page->buffer[check_pos];
        indent_count++;
        check_pos++;
    }
    
    /* Check if we have enough space for newline + indentation */
    if (page->length + 1 + indent_count >= PAGE_SIZE - 1) return;
    
    /* Shift everything from line_start forward to make room for newline + indentation */
    for (i = page->length + indent_count; i >= line_start; i--) {
        if (i - 1 - indent_count >= line_start) {
            page->buffer[i] = page->buffer[i - 1 - indent_count];
        }
    }
    
    /* Now insert the indentation and newline */
    if (line_start > 0) {
        /* Not at beginning - insert indentation then newline */
        for (i = 0; i < indent_count; i++) {
            page->buffer[line_start + i] = indent_chars[i];
        }
        /* Add newline after indentation */
        page->buffer[line_start + indent_count] = '\n';
        /* Position cursor at end of indentation on new line */
        page->cursor_pos = line_start + indent_count;
    } else {
        /* At beginning of buffer */
        for (i = 0; i < indent_count; i++) {
            page->buffer[i] = indent_chars[i];
        }
        /* Add newline after indentation */
        page->buffer[indent_count] = '\n';
        /* Position cursor at end of indentation */
        page->cursor_pos = indent_count;
    }
    
    /* Update length */
    page->length += 1 + indent_count;
    
    /* Enter insert mode */
    set_mode(MODE_INSERT);
    refresh_screen();
}

/* Move cursor to end of line (vim '$' command) */
void move_to_end_of_line(void) {
    Page *page = pages[current_page];
    
    /* Find end of current line */
    while (page->cursor_pos < page->length && 
           page->buffer[page->cursor_pos] != '\n') {
        page->cursor_pos++;
    }
    
    /* If not at end of buffer and not on empty line, move back one 
     * to be on last character rather than newline */
    if (page->cursor_pos > 0 && 
        page->cursor_pos < page->length &&
        page->buffer[page->cursor_pos] == '\n' &&
        (page->cursor_pos == 0 || page->buffer[page->cursor_pos - 1] != '\n')) {
        page->cursor_pos--;
    }
    
    refresh_screen();
}

/* Move cursor to first non-whitespace character of line (vim '^' command) */
void move_to_first_non_whitespace(void) {
    Page *page = pages[current_page];
    int line_start;
    
    /* Find start of current line */
    line_start = page->cursor_pos;
    while (line_start > 0 && page->buffer[line_start - 1] != '\n') {
        line_start--;
    }
    
    /* Move to start of line first */
    page->cursor_pos = line_start;
    
    /* Skip whitespace to find first non-whitespace character */
    while (page->cursor_pos < page->length && 
           page->buffer[page->cursor_pos] != '\n' &&
           (page->buffer[page->cursor_pos] == ' ' || 
            page->buffer[page->cursor_pos] == '\t')) {
        page->cursor_pos++;
    }
    
    refresh_screen();
}

/* Move cursor to next word */
void move_word_forward(void) {
    Page *page = pages[current_page];
    int pos = page->cursor_pos;
    
    /* Skip current word (alphanumeric chars) */
    while (pos < page->length && 
           ((page->buffer[pos] >= 'a' && page->buffer[pos] <= 'z') ||
            (page->buffer[pos] >= 'A' && page->buffer[pos] <= 'Z') ||
            (page->buffer[pos] >= '0' && page->buffer[pos] <= '9'))) {
        pos++;
    }
    
    /* Skip whitespace and punctuation to find next word */
    while (pos < page->length && 
           !((page->buffer[pos] >= 'a' && page->buffer[pos] <= 'z') ||
             (page->buffer[pos] >= 'A' && page->buffer[pos] <= 'Z') ||
             (page->buffer[pos] >= '0' && page->buffer[pos] <= '9'))) {
        pos++;
    }
    
    page->cursor_pos = pos;
    refresh_screen();
}

/* Move cursor to previous word */
void move_word_backward(void) {
    Page *page = pages[current_page];
    int pos = page->cursor_pos;
    
    /* Move back one char to get off current position */
    if (pos > 0) pos--;
    
    /* Skip whitespace and punctuation backwards */
    while (pos > 0 && 
           !((page->buffer[pos] >= 'a' && page->buffer[pos] <= 'z') ||
             (page->buffer[pos] >= 'A' && page->buffer[pos] <= 'Z') ||
             (page->buffer[pos] >= '0' && page->buffer[pos] <= '9'))) {
        pos--;
    }
    
    /* Move to beginning of word */
    while (pos > 0 && 
           ((page->buffer[pos-1] >= 'a' && page->buffer[pos-1] <= 'z') ||
            (page->buffer[pos-1] >= 'A' && page->buffer[pos-1] <= 'Z') ||
            (page->buffer[pos-1] >= '0' && page->buffer[pos-1] <= '9'))) {
        pos--;
    }
    
    page->cursor_pos = pos;
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
            /* Skip all click handling if in graphics mode */
            if (graphics_mode_active) {
                /* Just update button state and continue */
                prev_left_button = left_button;
                continue;
            }
            
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
                Page *page = pages[current_page];
                
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
                            
                            /* Check if this is a command (starts with $) */
                            if (page->buffer[page->highlight_start] == '$') {
                                /* Execute the command */
                                execute_command(page, page->highlight_start, page->highlight_end);
                            }
                            /* Check if this is a link (starts with #) */
                            else if (page->buffer[page->highlight_start] == '#') {
                                /* Execute the link */
                                execute_link(page, page->highlight_start, page->highlight_end);
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
            /* In graphics mode, send raw deltas directly */
            if (graphics_mode_active) {
                extern void handle_graphics_mouse_raw(signed char dx, signed char dy);
                handle_graphics_mouse_raw((signed char)dx, (signed char)dy);
                /* Skip text mode processing */
                continue;
            }
            
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
            /* If in graphics mode, update graphics cursor instead */
            if (graphics_mode_active) {
                handle_graphics_mouse_move(mouse_x, mouse_y);
            } else {
                refresh_screen();
            }
        }
        
        /* Update previous button state for next frame */
        prev_left_button = left_button;
        
        }  /* End of if (byte_num == 3) */
    }  /* End of while loop reading serial data */
}

/* Non-blocking keyboard check */
/* Shift key state - must persist between calls */
static int shift_pressed = 0;
static int ctrl_pressed = 0;

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
    
    /* Check for Ctrl press/release (left ctrl = 0x1D) */
    if (keycode == 0x1D) {
        ctrl_pressed = 1;
        return 0;
    } else if (keycode == 0x9D) {  /* Ctrl release */
        ctrl_pressed = 0;
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
        
        /* Handle Ctrl combinations */
        if (ctrl_pressed && c == '[') {
            /* Ctrl-[ should be treated as ESC */
            return 27;
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

/* Text rendering functions for DISPI - use display driver interface */
static void dispi_draw_char(int x, int y, unsigned char c, unsigned char fg, unsigned char bg) {
    const unsigned char *char_data;
    int row, col;
    unsigned char byte;
    
    /* Get character bitmap from 6x8 font */
    char_data = font_hp100lx_6x8[c];
    
    for (row = 0; row < FONT_hp100lx_HEIGHT; row++) {
        byte = char_data[row];
        
        /* Draw 6 columns */
        for (col = 0; col < FONT_hp100lx_WIDTH; col++) {
            if (byte & (0x80 >> col)) {
                display_set_pixel(x + col, y + row, fg);
            } else if (bg != 255) {  /* 255 = transparent */
                display_set_pixel(x + col, y + row, bg);
            }
        }
    }
}

static void dispi_draw_string(int x, int y, const char *str, unsigned char fg, unsigned char bg) {
    while (*str) {
        dispi_draw_char(x, y, (unsigned char)*str, fg, bg);
        x += FONT_hp100lx_WIDTH;
        str++;
    }
}

/* Test DISPI driver - recreate graphics demo using new display driver interface */
void test_dispi_driver(void) {
    DisplayDriver *driver;
    int running = 1;
    unsigned int current_time;
    int key;
    int i, x, y;
    unsigned char aquinas_palette[16][3];
    int cursor_x = 20, cursor_y = 50;  /* Cursor position for text input */
    int cursor_visible = 1;
    unsigned int cursor_blink_time = 0;
    char input_buffer[80];
    int input_len = 0;
    
    /* Mouse state for DISPI mode */
    static int dispi_mouse_x = 320;
    static int dispi_mouse_y = 240;
    static unsigned char mouse_bytes[3];
    static int mouse_byte_num = 0;
    
    serial_write_string("Starting DISPI driver demo\n");
    
    /* Save VGA font before switching to graphics mode */
    save_vga_font();
    
    /* Get the DISPI driver and set it as active */
    driver = dispi_get_driver();
    display_set_driver(driver);
    
    /* Enable double buffering for smooth rendering */
    if (!dispi_init_double_buffer()) {
        serial_write_string("WARNING: Double buffering failed, using single buffer\n");
    }
    
    /* Simple test: fill screen with red first */
    serial_write_string("Testing basic framebuffer fill...\n");
    display_clear(4);  /* Fill with light gray to test basic framebuffer access */
    
    /* Flip buffers to show initial clear */
    if (dispi_is_double_buffered()) {
        dispi_flip_buffers();
    }
    
    /* Wait a moment to see if basic fill works */
    for (i = 0; i < 10000000; i++) {
        __asm__ volatile("nop");
    }
    
    /* Set up Aquinas palette */
    /* Grayscale (0-5) */
    aquinas_palette[0][0] = 0x00; aquinas_palette[0][1] = 0x00; aquinas_palette[0][2] = 0x00;  /* Black */
    aquinas_palette[1][0] = 0x20; aquinas_palette[1][1] = 0x20; aquinas_palette[1][2] = 0x20;  /* Dark gray */
    aquinas_palette[2][0] = 0x38; aquinas_palette[2][1] = 0x38; aquinas_palette[2][2] = 0x38;  /* Medium dark gray */
    aquinas_palette[3][0] = 0x50; aquinas_palette[3][1] = 0x50; aquinas_palette[3][2] = 0x50;  /* Medium gray */
    aquinas_palette[4][0] = 0x70; aquinas_palette[4][1] = 0x70; aquinas_palette[4][2] = 0x70;  /* Light gray */
    aquinas_palette[5][0] = 0xE8; aquinas_palette[5][1] = 0xE8; aquinas_palette[5][2] = 0xE8;  /* White */
    
    /* Reds (6-8) */
    aquinas_palette[6][0] = 0x60; aquinas_palette[6][1] = 0x10; aquinas_palette[6][2] = 0x10;  /* Dark red */
    aquinas_palette[7][0] = 0xA0; aquinas_palette[7][1] = 0x20; aquinas_palette[7][2] = 0x20;  /* Medium red */
    aquinas_palette[8][0] = 0xE0; aquinas_palette[8][1] = 0x40; aquinas_palette[8][2] = 0x40;  /* Bright red */
    
    /* Golds (9-11) */
    aquinas_palette[9][0] = 0x80; aquinas_palette[9][1] = 0x60; aquinas_palette[9][2] = 0x20;  /* Dark gold */
    aquinas_palette[10][0] = 0xC0; aquinas_palette[10][1] = 0xA0; aquinas_palette[10][2] = 0x40; /* Medium gold */
    aquinas_palette[11][0] = 0xFF; aquinas_palette[11][1] = 0xE0; aquinas_palette[11][2] = 0x60; /* Bright yellow-gold */
    
    /* Cyans (12-14) */
    aquinas_palette[12][0] = 0x20; aquinas_palette[12][1] = 0x60; aquinas_palette[12][2] = 0x80; /* Dark cyan */
    aquinas_palette[13][0] = 0x40; aquinas_palette[13][1] = 0xA0; aquinas_palette[13][2] = 0xC0; /* Medium cyan */
    aquinas_palette[14][0] = 0x60; aquinas_palette[14][1] = 0xE0; aquinas_palette[14][2] = 0xFF; /* Bright cyan */
    
    /* Background (15) */
    aquinas_palette[15][0] = 0x48; aquinas_palette[15][1] = 0x48; aquinas_palette[15][2] = 0x48; /* Background gray */
    
    /* Set the palette */
    if (driver->set_palette) {
        driver->set_palette(aquinas_palette);
    }
    
    /* Clear screen with medium gray background */
    display_clear(15);  /* Use color 15 for background */
    
    /* Draw title text using DISPI text rendering */
    dispi_draw_string(20, 10, "DISPI Graphics Demo with Optimized Rendering", 0, 255);
    
    /* Draw instructions */
    dispi_draw_string(20, 25, "Type text below. Press ESC to exit. F = Fill test", 5, 255);
    
    /* Draw text input area */
    display_fill_rect(20, 48, 600, 20, 0);  /* Black input area */
    
    /* Initialize input buffer */
    for (i = 0; i < 80; i++) {
        input_buffer[i] = '\0';
    }
    
    /* Grayscale showcase */
    display_fill_rect(20, 80, 60, 60, 0);   /* Black */
    display_fill_rect(90, 80, 60, 60, 1);   /* Dark gray */
    display_fill_rect(160, 80, 60, 60, 2);  /* Medium dark gray */
    display_fill_rect(230, 80, 60, 60, 3);  /* Medium gray */
    display_fill_rect(300, 80, 60, 60, 4);  /* Light gray */
    display_fill_rect(370, 80, 60, 60, 5);  /* White */
    
    /* Red showcase */
    display_fill_rect(20, 160, 100, 50, 6);   /* Dark red */
    display_fill_rect(130, 160, 100, 50, 7);  /* Medium red */
    display_fill_rect(240, 160, 100, 50, 8);  /* Bright red */
    
    /* Gold showcase */
    display_fill_rect(20, 230, 100, 50, 9);   /* Dark gold */
    display_fill_rect(130, 230, 100, 50, 10); /* Medium gold */
    display_fill_rect(240, 230, 100, 50, 11); /* Bright yellow-gold */
    
    /* Cyan showcase */
    display_fill_rect(20, 300, 100, 50, 12);  /* Dark cyan */
    display_fill_rect(130, 300, 100, 50, 13); /* Medium cyan */
    display_fill_rect(240, 300, 100, 50, 14); /* Bright cyan */
    
    /* Draw some sample text to demonstrate rendering */
    dispi_draw_string(20, 365, "Sample Text: The quick brown fox jumps over the lazy dog.", 11, 0);
    dispi_draw_string(20, 375, "Colors: ", 5, 255);
    dispi_draw_string(70, 375, "Red ", 8, 255);
    dispi_draw_string(100, 375, "Gold ", 11, 255);
    dispi_draw_string(135, 375, "Cyan ", 14, 255);
    dispi_draw_string(170, 375, "White", 5, 255);
    
    /* Draw initial cursor in text input area */
    cursor_blink_time = get_ticks();
    display_fill_rect(cursor_x + 2, cursor_y + 6, 6, 2, 11);  /* Yellow underline cursor */
    
    /* Initialize and show mouse cursor */
    dispi_cursor_init();
    dispi_cursor_show();
    
    /* Flip buffers to show initial screen */
    if (dispi_is_double_buffered()) {
        dispi_flip_buffers();
    }
    
    serial_write_string("DISPI demo displayed. Mouse cursor active. Press ESC to exit.\n");
    
    /* Main loop */
    while (running) {
        /* Check for mouse input on COM1 */
        if (inb(0x3FD) & 0x01) {
            unsigned char data = inb(0x3F8);
            signed char dx, dy;
            
            /* Microsoft Serial Mouse protocol parsing */
            if (data & 0x40) {
                /* Start of packet (bit 6 set) */
                mouse_bytes[0] = data;
                mouse_byte_num = 1;
            } else if (mouse_byte_num > 0) {
                mouse_bytes[mouse_byte_num++] = data;
                
                if (mouse_byte_num == 3) {
                    /* Complete packet received */
                    dx = ((mouse_bytes[0] & 0x03) << 6) | (mouse_bytes[1] & 0x3F);
                    dy = ((mouse_bytes[0] & 0x0C) << 4) | (mouse_bytes[2] & 0x3F);
                    
                    /* Sign extend */
                    if (dx & 0x80) dx |= 0xFFFFFF00;
                    if (dy & 0x80) dy |= 0xFFFFFF00;
                    
                    /* Update mouse position (2x speed) */
                    dispi_mouse_x += dx * 2;
                    dispi_mouse_y += dy * 2;  /* Don't invert Y */
                    
                    /* Clamp to screen bounds */
                    if (dispi_mouse_x < 0) dispi_mouse_x = 0;
                    if (dispi_mouse_x >= 640) dispi_mouse_x = 639;
                    if (dispi_mouse_y < 0) dispi_mouse_y = 0;
                    if (dispi_mouse_y >= 480) dispi_mouse_y = 479;
                    
                    /* Update cursor position */
                    dispi_cursor_move(dispi_mouse_x, dispi_mouse_y);
                    
                    mouse_byte_num = 0;
                }
            }
        }
        
        /* Check for keyboard input using keyboard_check */
        key = keyboard_check();
        if (key == 27) { /* ESC - ASCII value */
            running = 0;
            serial_write_string("ESC pressed, exiting DISPI demo\n");
        } else if (key == 8 && input_len > 0) {
            /* Backspace - erase last character */
            input_len--;
            input_buffer[input_len] = '\0';
            cursor_x -= 6;
            /* Erase character and cursor */
            display_fill_rect(cursor_x + 2, cursor_y, 6, 8, 0);
        } else if (key == 13) {
            /* Enter - clear input and move to new line */
            display_fill_rect(20, 48, 600, 20, 0);  /* Clear input area */
            cursor_x = 20;
            input_len = 0;
            for (i = 0; i < 80; i++) {
                input_buffer[i] = '\0';
            }
        } else if (key == 'F' || key == 'f') {
            /* Fill test - compare regular vs optimized fills */
            unsigned int start_time, end_time;
            int test_x = 350, test_y = 160;
            int test_rects = 100;
            int rect;
            
            /* Test regular fill */
            dispi_draw_string(test_x, test_y, "Testing regular fill...", 5, 0);
            if (dispi_is_double_buffered()) {
                dispi_flip_buffers();
            }
            
            start_time = get_ticks();
            for (rect = 0; rect < test_rects; rect++) {
                display_fill_rect(test_x + (rect % 10) * 2, 
                                  test_y + 20 + (rect / 10) * 2, 
                                  20, 20, rect % 16);
            }
            end_time = get_ticks();
            
            dispi_draw_string(test_x, test_y + 45, "Regular: ", 5, 0);
            /* Simple number display - just show the difference */
            {
                char num_str[10];
                unsigned int diff = end_time - start_time;
                int idx = 0;
                if (diff == 0) {
                    num_str[0] = '0';
                    idx = 1;
                } else {
                    int temp = diff;
                    while (temp > 0 && idx < 9) {
                        num_str[idx++] = '0' + (temp % 10);
                        temp /= 10;
                    }
                    /* Reverse the string */
                    {
                        int j;
                        for (j = 0; j < idx/2; j++) {
                            char tmp = num_str[j];
                            num_str[j] = num_str[idx-1-j];
                            num_str[idx-1-j] = tmp;
                        }
                    }
                }
                num_str[idx] = '\0';
                dispi_draw_string(test_x + 60, test_y + 45, num_str, 11, 0);
                dispi_draw_string(test_x + 60 + idx*6, test_y + 45, " ms", 5, 0);
            }
            
            /* Test optimized fill */
            dispi_draw_string(test_x, test_y + 60, "Testing optimized fill...", 5, 0);
            if (dispi_is_double_buffered()) {
                dispi_flip_buffers();
            }
            
            start_time = get_ticks();
            for (rect = 0; rect < test_rects; rect++) {
                dispi_fill_rect_fast(test_x + (rect % 10) * 2,
                                     test_y + 80 + (rect / 10) * 2,
                                     20, 20, rect % 16);
            }
            end_time = get_ticks();
            
            dispi_draw_string(test_x, test_y + 105, "Optimized: ", 5, 0);
            /* Simple number display - just show the difference */
            {
                char num_str[10];
                unsigned int diff = end_time - start_time;
                int idx = 0;
                if (diff == 0) {
                    num_str[0] = '0';
                    idx = 1;
                } else {
                    int temp = diff;
                    while (temp > 0 && idx < 9) {
                        num_str[idx++] = '0' + (temp % 10);
                        temp /= 10;
                    }
                    /* Reverse the string */
                    {
                        int j;
                        for (j = 0; j < idx/2; j++) {
                            char tmp = num_str[j];
                            num_str[j] = num_str[idx-1-j];
                            num_str[idx-1-j] = tmp;
                        }
                    }
                }
                num_str[idx] = '\0';
                dispi_draw_string(test_x + 66, test_y + 105, num_str, 11, 0);
                dispi_draw_string(test_x + 66 + idx*6, test_y + 105, " ms", 5, 0);
            }
            
        } else if (key > 31 && key < 127 && input_len < 79) {
            /* Regular printable character */
            input_buffer[input_len] = (char)key;
            input_len++;
            
            /* Draw the character */
            dispi_draw_char(cursor_x + 2, cursor_y, (char)key, 11, 0);
            cursor_x += 6;
        }
        
        /* Update cursor blinking */
        current_time = get_ticks();
        if (current_time - cursor_blink_time >= 500) {
            cursor_visible = !cursor_visible;
            cursor_blink_time = current_time;
            
            if (cursor_visible) {
                /* Draw cursor */
                display_fill_rect(cursor_x + 2, cursor_y + 6, 6, 2, 11);
            } else {
                /* Erase cursor */
                display_fill_rect(cursor_x + 2, cursor_y + 6, 6, 2, 0);
            }
        }
        
        /* Flip buffers at end of frame if double buffering is enabled */
        if (dispi_is_double_buffered()) {
            dispi_flip_buffers();
        }
        
    }
    
    /* Return to text mode - save font, disable DISPI, restore VGA state */
    serial_write_string("Disabling DISPI and returning to text mode...\n");
    
    /* Hide mouse cursor before switching modes */
    dispi_cursor_hide();
    
    /* Cleanup double buffering if enabled */
    if (dispi_is_double_buffered()) {
        dispi_cleanup_double_buffer();
    }
    
    /* Disable DISPI first */
    dispi_disable();
    
    /* Restore the standard EGA/VGA palette before switching to text mode */
    restore_dac_palette();
    
    /* Now set standard VGA text mode */
    set_mode_03h();
    
    /* Restore the VGA font if it was saved */
    restore_vga_font();
    
    /* Clear screen to ensure clean text mode */
    vga_clear_screen();
    
    serial_write_string("Exited DISPI driver demo\n");
}

/* Helper function to draw a circle using DISPI */
static void draw_dispi_circle(int cx, int cy, int r, unsigned char color) {
    int x, y;
    int r2 = r * r;
    
    for (y = -r; y <= r; y++) {
        for (x = -r; x <= r; x++) {
            if (x*x + y*y <= r2) {
                /* Only draw the outline */
                if (x*x + y*y >= (r-2)*(r-2)) {
                    display_set_pixel(cx + x, cy + y, color);
                }
            }
        }
    }
}

void kernel_main(void) {
    int key;
    
    clear_screen();
    
    /* Initialize page management - must be done AFTER memory init */
    /* This is now handled by init_pages() */
    
    /* DEBUG: Test if highlighting works at all */
    /* pages[0]->highlight_start = 0;
    pages[0]->highlight_end = 5; */
    
    /* Initialize debug serial port (COM2) */
    init_debug_serial();
    serial_write_string("\n\nAquinas OS started!\n");
    serial_write_string("COM2 debug port initialized.\n");
    
    /* Initialize memory allocator */
    init_memory();
    
    /* Initialize pages (must be after memory init) */
    init_pages();
    serial_write_string("Pages initialized: allocated first page at ");
    serial_write_hex((unsigned int)pages[0]);
    serial_write_string(" with buffer at ");
    serial_write_hex((unsigned int)pages[0]->buffer);
    serial_write_string("\n");
    
    /* Report initial heap usage */
    serial_write_string("Initial heap usage: ");
    serial_write_int(get_heap_used());
    serial_write_string(" bytes (Page struct + ");
    serial_write_int(PAGE_SIZE);
    serial_write_string(" byte buffer)\n");
    
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
        static int last_key = 0;
        static unsigned int last_key_time = 0;
        static int pending_delete = 0;  /* For 'd' command sequences */
        static int pending_dt = 0;      /* For 'dt' command */
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
        
        /* Skip all keyboard processing if in graphics mode (ESC handled in graphics_demo) */
        if (graphics_mode_active) {
            continue;
        }
        
        /* Handle 'fd' escape sequence - insert 'f' immediately, delete if 'd' follows */
        if (editor_mode == MODE_INSERT) {
            /* Check if 'd' was typed shortly after 'f' */
            if (key == 'd' && last_key == 'f' && get_elapsed_ms(last_key_time) < FD_ESCAPE_TIMEOUT_MS) {
                /* 'fd' sequence detected - delete the 'f' we just inserted and exit */
                Page *page = pages[current_page];
                if (page->cursor_pos > 0 && page->buffer[page->cursor_pos - 1] == 'f') {
                    int i;
                    /* Delete the 'f' we just typed */
                    page->cursor_pos--;
                    page->length--;
                    /* Shift remaining text left */
                    for (i = page->cursor_pos; i < page->length; i++) {
                        page->buffer[i] = page->buffer[i + 1];
                    }
                    /* Refresh screen to show the 'f' was deleted */
                    refresh_screen();
                }
                /* Exit to normal mode */
                set_mode(MODE_NORMAL);
                key = 0;  /* Suppress the 'd' */
                last_key = 0;  /* Clear the 'f' marker */
            } else if (key == 'f') {
                /* Mark that we typed 'f' for potential 'fd' sequence */
                last_key = 'f';
                last_key_time = current_time;
                /* Let the 'f' be processed normally (inserted) below */
            } else if (key > 0) {
                /* Any other key - clear the 'f' marker */
                last_key = 0;
            }
        } else if (editor_mode == MODE_VISUAL) {
            /* For visual mode, just check for 'fd' to exit without insertion */
            if (key == 'd' && last_key == 'f' && get_elapsed_ms(last_key_time) < FD_ESCAPE_TIMEOUT_MS) {
                /* Exit to normal mode */
                key = 27;
                last_key = 0;
            } else if (key == 'f') {
                last_key = 'f';
                last_key_time = current_time;
                key = 0;  /* Don't process 'f' in visual mode */
            }
        }
        
        /* Handle mode-specific key bindings */
        if (editor_mode == MODE_NORMAL) {
            /* Normal mode - vim navigation and commands */
            if (key == 'h' || key == -3) {  /* h or Left arrow */
                if (!pending_delete && !pending_dt) {
                    move_cursor_left();
                }
                pending_delete = 0;  /* Cancel pending operations */
                pending_dt = 0;
            } else if (key == 'j' || key == -2) {  /* j or Down arrow */
                if (!pending_delete && !pending_dt) {
                    move_cursor_down();
                }
                pending_delete = 0;
                pending_dt = 0;
            } else if (key == 'k' || key == -1) {  /* k or Up arrow */
                if (!pending_delete && !pending_dt) {
                    move_cursor_up();
                }
                pending_delete = 0;
                pending_dt = 0;
            } else if (key == 'l' || key == -4) {  /* l or Right arrow */
                if (!pending_delete && !pending_dt) {
                    move_cursor_right();
                }
                pending_delete = 0;
                pending_dt = 0;
            } else if (key == 'i') {  /* Enter insert mode */
                set_mode(MODE_INSERT);
                pending_delete = 0;
                pending_dt = 0;
            } else if (key == 'a') {  /* Append - move right then insert */
                move_cursor_right();
                set_mode(MODE_INSERT);
                pending_delete = 0;
                pending_dt = 0;
            } else if (key == 'v') {  /* Enter visual mode */
                Page *page = pages[current_page];
                page->highlight_start = page->cursor_pos;
                page->highlight_end = page->cursor_pos;
                set_mode(MODE_VISUAL);
                pending_delete = 0;
                pending_dt = 0;
            } else if (key == 'x') {  /* Delete character under cursor */
                delete_char();
                pending_delete = 0;
                pending_dt = 0;
            } else if (key == 'd') {  /* Delete command */
                if (pending_delete) {
                    /* 'dd' - delete line */
                    delete_line();
                    pending_delete = 0;
                } else {
                    /* First 'd' - wait for next key */
                    pending_delete = 1;
                }
            } else if (key == 't' && pending_delete) {
                /* 'dt' - delete till character */
                pending_dt = 1;
                pending_delete = 0;
            } else if (key == '$' && pending_delete) {
                /* 'd$' - delete to end of line */
                delete_to_eol();
                pending_delete = 0;
            } else if (key == '^' && pending_delete) {
                /* 'd^' - delete to beginning of line (first non-whitespace) */
                delete_to_bol();
                pending_delete = 0;
            } else if (pending_dt && key > 0 && key < 127) {
                /* Complete 'dt<char>' command */
                delete_till_char((char)key);
                pending_dt = 0;
            } else if (key == 'w') {  /* Move forward by word */
                if (!pending_delete) {
                    move_word_forward();
                }
                pending_delete = 0;
                pending_dt = 0;
            } else if (key == 'b') {  /* Move backward by word */
                if (!pending_delete) {
                    move_word_backward();
                }
                pending_delete = 0;
                pending_dt = 0;
            } else if (key == 'o') {  /* Insert line below and enter insert mode */
                insert_line_below();
                pending_delete = 0;
                pending_dt = 0;
            } else if (key == 'O') {  /* Insert line above and enter insert mode */
                insert_line_above();
                pending_delete = 0;
                pending_dt = 0;
            } else if (key == '$' && !pending_delete) {  /* Move to end of line */
                move_to_end_of_line();
                pending_dt = 0;
            } else if (key == '^') {  /* Move to first non-whitespace character */
                if (!pending_delete) {
                    move_to_first_non_whitespace();
                }
                pending_delete = 0;
                pending_dt = 0;
            } else if (key == -5) {  /* Shift+Left = Previous page */
                prev_page();
            } else if (key == -6) {  /* Shift+Right = Next page */
                next_page();
            }
        } else if (editor_mode == MODE_INSERT) {
            /* Insert mode - regular typing */
            if (key == 27) {  /* ESC - return to normal mode */
                set_mode(MODE_NORMAL);
            } else if (key == -1) {  /* Up arrow */
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
            } else if (key > 0) {  /* Regular character */
                insert_char((char)key);
            }
        } else if (editor_mode == MODE_VISUAL) {
            /* Visual mode - selection and movement */
            Page *page = pages[current_page];
            if (key == 27) {  /* ESC - return to normal mode */
                page->highlight_start = 0;
                page->highlight_end = 0;
                set_mode(MODE_NORMAL);
                refresh_screen();
            } else if (key == 'h' || key == -3) {  /* h or Left arrow */
                move_cursor_left();
                page->highlight_end = page->cursor_pos;
                refresh_screen();
            } else if (key == 'j' || key == -2) {  /* j or Down arrow */
                move_cursor_down();
                page->highlight_end = page->cursor_pos;
                refresh_screen();
            } else if (key == 'k' || key == -1) {  /* k or Up arrow */
                move_cursor_up();
                page->highlight_end = page->cursor_pos;
                refresh_screen();
            } else if (key == 'l' || key == -4) {  /* l or Right arrow */
                move_cursor_right();
                page->highlight_end = page->cursor_pos;
                refresh_screen();
            }
        }
    }
}
