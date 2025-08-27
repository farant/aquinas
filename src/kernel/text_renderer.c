/* Text rendering implementation for graphics mode */

#include "text_renderer.h"
#include "display_driver.h"
#include "font_6x8.h"
#include "timer.h"
#include "serial.h"

/* Global text renderer instance */
static TextRenderer text_renderer;

/* Cursor blink rate in milliseconds */
#define CURSOR_BLINK_RATE 500

/* Initialize text renderer */
void text_renderer_init(void) {
    text_renderer.cursor_x = 0;
    text_renderer.cursor_y = 0;
    text_renderer.fg_color = 5;  /* White */
    text_renderer.bg_color = 0;  /* Black */
    text_renderer.cursor_visible = 1;
    text_renderer.cursor_blink_state = 1;
    text_renderer.last_blink_time = get_ticks();
    
    serial_write_string("Text renderer initialized\n");
}

/* Clear the screen */
void text_renderer_clear(void) {
    display_clear(text_renderer.bg_color);
    text_renderer.cursor_x = 0;
    text_renderer.cursor_y = 0;
}

/* Draw a character at specific position */
void text_renderer_draw_char(int col, int row, char c, unsigned char fg, unsigned char bg) {
    int x, y, bit;
    int pixel_x = col * FONT_hp100lx_WIDTH;
    int pixel_y = row * FONT_hp100lx_HEIGHT;
    unsigned char byte;
    
    /* Bounds checking */
    if (col < 0 || col >= TEXT_COLS || row < 0 || row >= TEXT_ROWS) {
        return;
    }
    
    /* Clear background first */
    display_fill_rect(pixel_x, pixel_y, FONT_hp100lx_WIDTH, FONT_hp100lx_HEIGHT, bg);
    
    /* Draw the character bitmap */
    for (y = 0; y < FONT_hp100lx_HEIGHT; y++) {
        byte = font_hp100lx_6x8[(unsigned char)c][y];
        for (x = 0; x < FONT_hp100lx_WIDTH; x++) {
            bit = (byte >> (7 - x)) & 1;
            if (bit) {
                display_set_pixel(pixel_x + x, pixel_y + y, fg);
            }
        }
    }
}

/* Draw a string at specific position */
void text_renderer_draw_string(int col, int row, const char *str, unsigned char fg, unsigned char bg) {
    int i = 0;
    while (str[i] && col + i < TEXT_COLS) {
        text_renderer_draw_char(col + i, row, str[i], fg, bg);
        i++;
    }
}

/* Draw a character at the current cursor position */
void text_renderer_putchar(char c) {
    if (c == '\n') {
        /* Newline */
        text_renderer.cursor_x = 0;
        text_renderer.cursor_y++;
        if (text_renderer.cursor_y >= TEXT_ROWS) {
            text_renderer_scroll();
            text_renderer.cursor_y = TEXT_ROWS - 1;
        }
    } else if (c == '\r') {
        /* Carriage return */
        text_renderer.cursor_x = 0;
    } else if (c == '\t') {
        /* Tab - move to next multiple of 4 */
        text_renderer.cursor_x = (text_renderer.cursor_x + 4) & ~3;
        if (text_renderer.cursor_x >= TEXT_COLS) {
            text_renderer.cursor_x = 0;
            text_renderer.cursor_y++;
            if (text_renderer.cursor_y >= TEXT_ROWS) {
                text_renderer_scroll();
                text_renderer.cursor_y = TEXT_ROWS - 1;
            }
        }
    } else if (c == '\b') {
        /* Backspace */
        if (text_renderer.cursor_x > 0) {
            text_renderer.cursor_x--;
            text_renderer_draw_char(text_renderer.cursor_x, text_renderer.cursor_y, 
                                   ' ', text_renderer.fg_color, text_renderer.bg_color);
        }
    } else {
        /* Normal character */
        text_renderer_draw_char(text_renderer.cursor_x, text_renderer.cursor_y, 
                               c, text_renderer.fg_color, text_renderer.bg_color);
        text_renderer.cursor_x++;
        if (text_renderer.cursor_x >= TEXT_COLS) {
            text_renderer.cursor_x = 0;
            text_renderer.cursor_y++;
            if (text_renderer.cursor_y >= TEXT_ROWS) {
                text_renderer_scroll();
                text_renderer.cursor_y = TEXT_ROWS - 1;
            }
        }
    }
}

/* Draw a string at the current cursor position */
void text_renderer_puts(const char *str) {
    while (*str) {
        text_renderer_putchar(*str++);
    }
}

/* Set cursor position */
void text_renderer_set_cursor(int col, int row) {
    if (col >= 0 && col < TEXT_COLS) {
        text_renderer.cursor_x = col;
    }
    if (row >= 0 && row < TEXT_ROWS) {
        text_renderer.cursor_y = row;
    }
}

/* Get cursor position */
void text_renderer_get_cursor(int *col, int *row) {
    if (col) *col = text_renderer.cursor_x;
    if (row) *row = text_renderer.cursor_y;
}

/* Show/hide cursor */
void text_renderer_show_cursor(int visible) {
    text_renderer.cursor_visible = visible;
    if (!visible) {
        /* Erase cursor if hiding */
        int pixel_x = text_renderer.cursor_x * FONT_hp100lx_WIDTH;
        int pixel_y = text_renderer.cursor_y * FONT_hp100lx_HEIGHT;
        display_fill_rect(pixel_x, pixel_y + FONT_hp100lx_HEIGHT - 2, 
                         FONT_hp100lx_WIDTH, 2, text_renderer.bg_color);
    }
}

/* Update cursor blink */
void text_renderer_update_cursor(void) {
    unsigned int current_time;
    int pixel_x, pixel_y;
    
    if (!text_renderer.cursor_visible) {
        return;
    }
    
    current_time = get_ticks();
    if (current_time - text_renderer.last_blink_time >= CURSOR_BLINK_RATE) {
        text_renderer.cursor_blink_state = !text_renderer.cursor_blink_state;
        text_renderer.last_blink_time = current_time;
        
        pixel_x = text_renderer.cursor_x * FONT_hp100lx_WIDTH;
        pixel_y = text_renderer.cursor_y * FONT_hp100lx_HEIGHT;
        
        if (text_renderer.cursor_blink_state) {
            /* Draw cursor */
            display_fill_rect(pixel_x, pixel_y + FONT_hp100lx_HEIGHT - 2, 
                             FONT_hp100lx_WIDTH, 2, text_renderer.fg_color);
        } else {
            /* Erase cursor */
            display_fill_rect(pixel_x, pixel_y + FONT_hp100lx_HEIGHT - 2, 
                             FONT_hp100lx_WIDTH, 2, text_renderer.bg_color);
        }
    }
}

/* Draw the cursor at current position */
void text_renderer_draw_cursor(void) {
    int pixel_x, pixel_y;
    
    if (!text_renderer.cursor_visible || !text_renderer.cursor_blink_state) {
        return;
    }
    
    pixel_x = text_renderer.cursor_x * FONT_hp100lx_WIDTH;
    pixel_y = text_renderer.cursor_y * FONT_hp100lx_HEIGHT;
    
    /* Draw cursor as underline */
    display_fill_rect(pixel_x, pixel_y + FONT_hp100lx_HEIGHT - 2, 
                     FONT_hp100lx_WIDTH, 2, text_renderer.fg_color);
}

/* Set text colors */
void text_renderer_set_colors(unsigned char fg, unsigned char bg) {
    text_renderer.fg_color = fg;
    text_renderer.bg_color = bg;
}

/* Scroll the screen up one line */
void text_renderer_scroll(void) {
    int row, col;
    unsigned char *scanline = (unsigned char *)0x20000; /* Temporary buffer */
    DisplayDriver *driver = display_get_driver();
    int y;
    
    /* Copy each row up by one */
    for (row = 0; row < TEXT_ROWS - 1; row++) {
        int src_y = (row + 1) * FONT_hp100lx_HEIGHT;
        int dst_y = row * FONT_hp100lx_HEIGHT;
        
        /* Copy scanlines for this text row */
        for (y = 0; y < FONT_hp100lx_HEIGHT; y++) {
            /* Read source scanline */
            for (col = 0; col < driver->width; col++) {
                scanline[col] = driver->get_pixel(col, src_y + y);
            }
            /* Write to destination */
            for (col = 0; col < driver->width; col++) {
                driver->set_pixel(col, dst_y + y, scanline[col]);
            }
        }
    }
    
    /* Clear the bottom row */
    display_fill_rect(0, (TEXT_ROWS - 1) * FONT_hp100lx_HEIGHT, 
                     driver->width, FONT_hp100lx_HEIGHT, text_renderer.bg_color);
}

/* Get the global text renderer instance */
TextRenderer* text_renderer_get(void) {
    return &text_renderer;
}