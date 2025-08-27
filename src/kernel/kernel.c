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
#include "grid.h"
#include "graphics_context.h"
#include "page.h"
#include "modes.h"
#include "display.h"
#include "commands.h"
#include "editor.h"
#include "input.h"

/* From graphics.c - VGA state functions */
void save_vga_font(void);
void restore_vga_font(void);
void restore_dac_palette(void);

/* Editor modes moved to modes.c */

/* Mouse state moved to display.c */

/* Forward declarations */
void test_dispi_driver(void);
static void draw_dispi_circle(int cx, int cy, int r, unsigned char color);

/* Page management functions moved to page.c */
unsigned int get_stack_usage(void);

/* Port I/O functions now in io.h */

/* Mode management functions moved to modes.c */

/* Page navigation functions moved to page.c */

/* Display functions moved to display.c */

/* Command functions moved to commands.c */

/* Editor functions moved to editor.c */

/* Old blocking keyboard handler - kept for reference but not used */
/* keyboard_get_scancode was replaced by non-blocking keyboard_check */

/* Input functions moved to input.c */

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
    
    /* Graphics test state */
    int graphics_test_visible = 0;
    int grid_test_visible = 0;
    int context_test_visible = 0;
    int last_hover_col = -1;  /* Track last highlighted cell */
    int last_hover_row = -1;
    
    serial_write_string("Starting DISPI driver demo\n");
    
    /* Initialize grid system */
    grid_init();
    
    /* Save VGA font before switching to graphics mode */
    save_vga_font();
    
    /* Get the DISPI driver and set it as active */
    driver = dispi_get_driver();
    serial_write_string("Got driver pointer: ");
    serial_write_hex((unsigned int)driver);
    serial_write_string(" with name ptr: ");
    serial_write_hex((unsigned int)(driver ? driver->name : 0));
    serial_write_string("\n");
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
    dispi_draw_string(20, 25, "ESC=exit, F=Fill, G=Graphics, R=Grid test", 5, 255);
    
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
                    
                    /* If grid test is visible, highlight cell under mouse */
                    if (grid_test_visible) {
                        int hover_col, hover_row;
                        grid_pixel_to_cell(dispi_mouse_x, dispi_mouse_y, &hover_col, &hover_row);
                        
                        /* Check if we moved to a different cell */
                        if (hover_col != last_hover_col || hover_row != last_hover_row) {
                            /* Restore previous cell if any */
                            if (last_hover_col >= 0 && last_hover_row >= 0) {
                                /* Redraw the cell area with black background */
                                grid_draw_cell_filled(last_hover_col, last_hover_row, 0);
                                /* Redraw grid lines around it */
                                if (last_hover_col > 0 && last_hover_col % CELLS_PER_REGION_X != 0) {
                                    int x, y;
                                    int cell_region;
                                    grid_cell_to_pixel(last_hover_col, last_hover_row, &x, &y);
                                    /* Adjust for bar */
                                    cell_region = last_hover_col / CELLS_PER_REGION_X;
                                    if (grid_get_bar_position() >= 0 && cell_region > grid_get_bar_position()) {
                                        x += BAR_WIDTH;
                                    }
                                    dispi_draw_line(x, y, x, y + CELL_HEIGHT - 1, 1);
                                }
                                if (last_hover_row > 0 && last_hover_row % CELLS_PER_REGION_Y != 0) {
                                    int x, y;
                                    int cell_region;
                                    grid_cell_to_pixel(last_hover_col, last_hover_row, &x, &y);
                                    /* Adjust for bar */
                                    cell_region = last_hover_col / CELLS_PER_REGION_X;
                                    if (grid_get_bar_position() >= 0 && cell_region > grid_get_bar_position()) {
                                        x += BAR_WIDTH;
                                    }
                                    dispi_draw_line(x, y, x + CELL_WIDTH - 1, y, 1);
                                }
                            }
                            
                            /* Highlight new cell with dark red */
                            if (hover_col >= 0 && hover_col < CELLS_PER_ROW && 
                                hover_row >= 0 && hover_row < CELLS_PER_COL) {
                                grid_draw_cell_filled(hover_col, hover_row, 6);  /* Dark red */
                                last_hover_col = hover_col;
                                last_hover_row = hover_row;
                            }
                            
                            /* Flip buffers to show change */
                            if (dispi_is_double_buffered()) {
                                dispi_flip_buffers();
                            }
                        }
                    }
                    
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
            /* First erase old cursor at current position */
            display_fill_rect(cursor_x + 2, cursor_y + 6, 6, 2, 0);
            
            input_len--;
            input_buffer[input_len] = '\0';
            cursor_x -= 6;
            
            /* Erase the character and any cursor remnants */
            display_fill_rect(cursor_x + 2, cursor_y, 6, 10, 0);  /* 8 for char + 2 for cursor underline */
            
            /* Reset cursor to visible state after moving */
            cursor_visible = 1;
            cursor_blink_time = current_time;
        } else if (key == 13) {
            /* Enter - clear input and move to new line */
            /* First erase old cursor */
            display_fill_rect(cursor_x + 2, cursor_y + 6, 6, 2, 0);
            
            display_fill_rect(20, 48, 600, 20, 0);  /* Clear input area */
            cursor_x = 20;
            input_len = 0;
            for (i = 0; i < 80; i++) {
                input_buffer[i] = '\0';
            }
            
            /* Reset cursor to visible state after clearing */
            cursor_visible = 1;
            cursor_blink_time = current_time;
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
            
        } else if (key == 'G' || key == 'g') {
            /* Toggle graphics primitives test */
            graphics_test_visible = !graphics_test_visible;
            
            if (graphics_test_visible) {
                /* Show graphics test */
                int test_x = 50, test_y = 50;
                
                /* Clear an area for our test */
                display_fill_rect(test_x - 10, test_y - 10, 540, 380, 15);
                
                /* Test line drawing */
                dispi_draw_string_bios(test_x, test_y, "Line Drawing Test:", 5, 255);
                
                /* Draw a box using lines */
                dispi_draw_line(test_x, test_y + 25, test_x + 100, test_y + 25, 8);  /* Top - red */
                dispi_draw_line(test_x + 100, test_y + 25, test_x + 100, test_y + 75, 10);  /* Right - gold */
                dispi_draw_line(test_x + 100, test_y + 75, test_x, test_y + 75, 13);  /* Bottom - cyan */
                dispi_draw_line(test_x, test_y + 75, test_x, test_y + 25, 14);  /* Left - bright cyan */
                
                /* Draw diagonal lines */
                dispi_draw_line(test_x, test_y + 25, test_x + 100, test_y + 75, 6);  /* Diagonal \ - dark red */
                dispi_draw_line(test_x, test_y + 75, test_x + 100, test_y + 25, 11);  /* Diagonal / - bright gold */
                
                /* Test circle drawing */
                dispi_draw_string_bios(test_x + 150, test_y, "Circle Drawing Test:", 5, 255);
                
                /* Draw concentric circles */
                dispi_draw_circle(test_x + 200, test_y + 50, 30, 12);  /* Dark cyan */
                dispi_draw_circle(test_x + 200, test_y + 50, 20, 13);  /* Medium cyan */
                dispi_draw_circle(test_x + 200, test_y + 50, 10, 14);  /* Bright cyan */
                dispi_draw_circle(test_x + 200, test_y + 50, 5, 5);    /* White center */
                
                /* Test BIOS font with different colors */
                dispi_draw_string_bios(test_x, test_y + 100, "BIOS 9x16 Font Test:", 5, 255);
                dispi_draw_string_bios(test_x, test_y + 120, "The quick brown fox jumps over the lazy dog.", 11, 255);
                dispi_draw_string_bios(test_x, test_y + 140, "AQUINAS OS - Text Editor Operating System", 8, 0);
                dispi_draw_string_bios(test_x, test_y + 160, "0123456789 !@#$%^&*() []{}|\\;:'\",.<>?/", 10, 255);
                
                /* Draw some box drawing characters if available */
                dispi_draw_string_bios(test_x, test_y + 190, "Box Drawing:", 5, 255);
                {
                    int i;
                    unsigned char box_chars[] = {
                        0xC9, 0xCD, 0xCD, 0xCD, 0xCD, 0xCB, 0xCD, 0xCD, 0xCD, 0xCD, 0xBB, 0,  /* Top border */
                        0xBA, ' ', ' ', ' ', ' ', 0xBA, ' ', ' ', ' ', ' ', 0xBA, 0,           /* Middle */
                        0xC8, 0xCD, 0xCD, 0xCD, 0xCD, 0xCA, 0xCD, 0xCD, 0xCD, 0xCD, 0xBC, 0    /* Bottom */
                    };
                    
                    for (i = 0; box_chars[i]; i++) {
                        if (box_chars[i] == 0) {
                            test_y += 16;
                            test_x = 50;
                        } else {
                            dispi_draw_char_bios(test_x + i * 9, test_y + 210, box_chars[i], 14, 255);
                        }
                    }
                }
                
                /* Draw a pattern test */
                dispi_draw_string_bios(test_x, test_y + 270, "Pattern Test with Lines:", 5, 255);
                {
                    int i;
                    for (i = 0; i < 16; i++) {
                        dispi_draw_line(test_x + i * 10, test_y + 290, 
                                        test_x + i * 10, test_y + 330, i);
                    }
                }
            } else {
                /* Hide graphics test - first clear the entire area */
                display_fill_rect(0, 48, 640, 400, 15);  /* Clear with background color */
                
                /* Redraw text input area */
                display_fill_rect(20, 48, 600, 20, 0);  /* Black input area */
                
                /* Redraw input text and cursor if any */
                if (input_len > 0) {
                    int temp_x = 20;
                    int j;
                    for (j = 0; j < input_len; j++) {
                        dispi_draw_char(temp_x + 2, 50, input_buffer[j], 11, 0);
                        temp_x += 6;
                    }
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
                
                /* Draw sample text */
                dispi_draw_string(20, 365, "Sample Text: The quick brown fox jumps over the lazy dog.", 11, 0);
                dispi_draw_string(20, 375, "Colors: ", 5, 255);
                dispi_draw_string(70, 375, "Red ", 8, 255);
                dispi_draw_string(100, 375, "Gold ", 11, 255);
                dispi_draw_string(135, 375, "Cyan ", 14, 255);
                dispi_draw_string(170, 375, "White", 5, 255);
            }
            
            /* Flip buffers to show changes */
            if (dispi_is_double_buffered()) {
                dispi_flip_buffers();
            }
            
        } else if (key == 'C' || key == 'c') {
            /* Toggle graphics context test */
            context_test_visible = !context_test_visible;
            
            if (context_test_visible) {
                /* Show graphics context test */
                GraphicsContext *gc1, *gc2, *gc3;
                Pattern8x8 checkerboard, stripes, dots;
                DisplayDriver *driver = display_get_driver();
                
                /* Clear screen */
                display_clear(0);  /* Black background */
                
                /* Create some patterns */
                pattern_create_checkerboard(&checkerboard);
                pattern_create_horizontal_stripes(&stripes, 2);
                pattern_create_dots(&dots, 3);
                
                /* Test 1: Different clip regions with translation */
                gc1 = gc_create(driver);
                if (gc1) {
                    /* Top-left quadrant with clipping */
                    gc_set_clip(gc1, 50, 50, 200, 150);
                    gc_set_colors(gc1, 14, 1); /* Bright cyan on blue */
                    gc_set_translation(gc1, 10, 10);
                    
                    /* Draw title */
                    dispi_draw_string_bios(50, 20, "Graphics Context Test - Press C to toggle", 11, 0);
                    dispi_draw_string_bios(50, 40, "Clip Region 1 (top-left)", 14, 0);
                    
                    /* Draw some shapes that will be clipped */
                    gc_fill_rect(gc1, 0, 0, 300, 200, gc1->fg_color);
                    gc_draw_rect(gc1, 5, 5, 190, 140, 8); /* Red border */
                    gc_draw_line(gc1, 0, 0, 200, 150, 15); /* White diagonal */
                    gc_draw_circle(gc1, 100, 75, 50, 10); /* Gold circle */
                    
                    gc_destroy(gc1);
                }
                
                /* Test 2: Pattern fills with different context */
                gc2 = gc_create(driver);
                if (gc2) {
                    /* Top-right quadrant */
                    gc_set_clip(gc2, 350, 50, 200, 150);
                    gc_set_colors(gc2, 12, 4); /* Red on dark red */
                    gc_set_translation(gc2, -300, 10);
                    
                    dispi_draw_string_bios(350, 40, "Pattern Fill Test", 12, 0);
                    
                    /* Test different patterns */
                    gc_set_pattern(gc2, &checkerboard);
                    gc_set_fill_mode(gc2, FILL_PATTERN);
                    gc_fill_rect_current_pattern(gc2, 350, 0, 80, 60);
                    
                    gc_set_pattern(gc2, &stripes);
                    gc_fill_rect_current_pattern(gc2, 430, 0, 80, 60);
                    
                    gc_set_pattern(gc2, &dots);
                    gc_fill_rect_current_pattern(gc2, 390, 60, 80, 60);
                    
                    gc_destroy(gc2);
                }
                
                /* Test 3: Multiple contexts on same region */
                gc3 = gc_create(driver);
                if (gc3) {
                    /* Bottom area with multiple overlapping contexts */
                    dispi_draw_string_bios(50, 220, "Overlapping Contexts", 10, 0);
                    
                    /* First context - large rectangle */
                    gc_set_clip(gc3, 50, 250, 500, 150);
                    gc_set_colors(gc3, 9, 0); /* Light blue */
                    gc_fill_rect(gc3, 50, 250, 200, 100, gc3->fg_color);
                    
                    /* Change context properties and draw more */
                    gc_set_clip(gc3, 150, 280, 300, 100);
                    gc_set_colors(gc3, 13, 5); /* Magenta on dark magenta */
                    gc_set_pattern(gc3, &checkerboard);
                    gc_set_fill_mode(gc3, FILL_PATTERN);
                    gc_fill_rect_current_pattern(gc3, 150, 280, 150, 80);
                    
                    /* Add some shapes with translation */
                    gc_set_translation(gc3, 200, 50);
                    gc_set_fill_mode(gc3, FILL_SOLID);
                    gc_set_colors(gc3, 15, 0); /* White */
                    gc_draw_circle(gc3, 50, 50, 30, gc3->fg_color);
                    gc_fill_circle(gc3, 150, 50, 25, 6);
                    
                    gc_destroy(gc3);
                }
                
                /* Show instructions */
                dispi_draw_string_bios(50, 420, "Context features: clipping, translation, patterns", 7, 0);
                dispi_draw_string_bios(50, 440, "Notice how shapes are clipped to their regions", 7, 0);
            } else {
                /* Clear context test */
                display_clear(0);
                dispi_draw_string_bios(50, 50, "Graphics Context Test Hidden", 5, 0);
            }
            
            /* Flip buffers to show changes */
            if (dispi_is_double_buffered()) {
                dispi_flip_buffers();
            }
            
        } else if (key == 'R' || key == 'r') {
            /* Toggle grid visualization */
            grid_test_visible = !grid_test_visible;
            
            if (grid_test_visible) {
                /* Show grid */
                /* First clear screen */
                display_clear(0);  /* Black background */
                
                /* Draw grid lines */
                grid_draw_grid_lines(1, 5);  /* Dark gray cells, white regions */
                
                /* Label some cells and regions */
                dispi_draw_string_bios(5, 5, "Grid System Test", 11, 255);
                dispi_draw_string_bios(5, 25, "Cells: 71x30 (9x16 px)", 14, 255);
                dispi_draw_string_bios(5, 45, "Regions: 7x6 (90x80 px)", 10, 255);
                
                /* Highlight specific cells */
                grid_draw_cell_outline(0, 0, 8);    /* Red - top-left cell */
                grid_draw_cell_outline(70, 0, 8);   /* Red - top-right cell */
                grid_draw_cell_outline(0, 29, 8);   /* Red - bottom-left cell */
                grid_draw_cell_outline(70, 29, 8);  /* Red - bottom-right cell */
                
                /* Highlight specific regions */
                grid_draw_region_outline(0, 0, 11);  /* Gold - top-left region */
                grid_draw_region_outline(6, 0, 11);  /* Gold - top-right region */
                grid_draw_region_outline(0, 5, 11);  /* Gold - bottom-left region */
                grid_draw_region_outline(6, 5, 11);  /* Gold - bottom-right region */
                
                /* Show bar position (if set) */
                {
                    int bar_x, bar_y, bar_w, bar_h;
                    char bar_info[40];
                    grid_get_bar_rect(&bar_x, &bar_y, &bar_w, &bar_h);
                    if (bar_x >= 0) {
                        dispi_draw_string_bios(5, 65, "Bar at column 3 (10px wide)", 11, 255);
                    }
                }
                
                /* Draw some content in specific cells to show alignment */
                {
                    int x, y;
                    /* Put characters at cell boundaries to verify alignment */
                    grid_cell_to_pixel(10, 10, &x, &y);
                    dispi_draw_char_bios(x, y, 'A', 14, 255);
                    
                    grid_cell_to_pixel(20, 10, &x, &y);
                    dispi_draw_char_bios(x, y, 'B', 13, 255);
                    
                    grid_cell_to_pixel(30, 10, &x, &y);
                    dispi_draw_char_bios(x, y, 'C', 12, 255);
                }
                
                /* Test region coordinate conversion */
                {
                    int px, py;
                    grid_region_to_pixel(3, 3, &px, &py);
                    dispi_draw_string_bios(px + 5, py + 5, "Region 3,3", 5, 0);
                }
                
            } else {
                /* Hide grid - restore normal demo */
                last_hover_col = -1;  /* Reset hover tracking */
                last_hover_row = -1;
                display_clear(15);  /* Medium gray background */
                
                /* Redraw title and instructions */
                dispi_draw_string(20, 10, "DISPI Graphics Demo with Optimized Rendering", 0, 255);
                dispi_draw_string(20, 25, "ESC=exit, F=Fill, G=Graphics, R=Grid test", 5, 255);
                
                /* Redraw text input area */
                display_fill_rect(20, 48, 600, 20, 0);
                
                /* Redraw input text if any */
                if (input_len > 0) {
                    int temp_x = 20;
                    int j;
                    for (j = 0; j < input_len; j++) {
                        dispi_draw_char(temp_x + 2, 50, input_buffer[j], 11, 0);
                        temp_x += 6;
                    }
                }
                
                /* Redraw color showcases */
                display_fill_rect(20, 80, 60, 60, 0);   /* Black */
                display_fill_rect(90, 80, 60, 60, 1);   /* Dark gray */
                display_fill_rect(160, 80, 60, 60, 2);  /* Medium dark gray */
                display_fill_rect(230, 80, 60, 60, 3);  /* Medium gray */
                display_fill_rect(300, 80, 60, 60, 4);  /* Light gray */
                display_fill_rect(370, 80, 60, 60, 5);  /* White */
                
                display_fill_rect(20, 160, 100, 50, 6);   /* Dark red */
                display_fill_rect(130, 160, 100, 50, 7);  /* Medium red */
                display_fill_rect(240, 160, 100, 50, 8);  /* Bright red */
                
                display_fill_rect(20, 230, 100, 50, 9);   /* Dark gold */
                display_fill_rect(130, 230, 100, 50, 10); /* Medium gold */
                display_fill_rect(240, 230, 100, 50, 11); /* Bright yellow-gold */
                
                display_fill_rect(20, 300, 100, 50, 12);  /* Dark cyan */
                display_fill_rect(130, 300, 100, 50, 13); /* Medium cyan */
                display_fill_rect(240, 300, 100, 50, 14); /* Bright cyan */
                
                dispi_draw_string(20, 365, "Sample Text: The quick brown fox jumps over the lazy dog.", 11, 0);
                dispi_draw_string(20, 375, "Colors: ", 5, 255);
                dispi_draw_string(70, 375, "Red ", 8, 255);
                dispi_draw_string(100, 375, "Gold ", 11, 255);
                dispi_draw_string(135, 375, "Cyan ", 14, 255);
                dispi_draw_string(170, 375, "White", 5, 255);
            }
            
            /* Flip buffers */
            if (dispi_is_double_buffered()) {
                dispi_flip_buffers();
            }
            
        } else if (key > 31 && key < 127 && input_len < 79) {
            /* Regular printable character */
            /* Erase old cursor before moving */
            display_fill_rect(cursor_x + 2, cursor_y + 6, 6, 2, 0);
            
            input_buffer[input_len] = (char)key;
            input_len++;
            
            /* Draw the character */
            dispi_draw_char(cursor_x + 2, cursor_y, (char)key, 11, 0);
            cursor_x += 6;
            
            /* Reset cursor to visible state after moving */
            cursor_visible = 1;
            cursor_blink_time = current_time;
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
