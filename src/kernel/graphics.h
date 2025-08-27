#ifndef GRAPHICS_H
#define GRAPHICS_H

#define VGA_GRAPHICS_BUFFER 0xA0000
#define VGA_WIDTH_12H 640
#define VGA_HEIGHT_12H 480
#define VGA_PLANES 4

/* Text spacing constants */
#define CHAR_WIDTH_TIGHT    8   /* No gap - characters touch */
#define CHAR_WIDTH_NORMAL   9   /* 1 pixel gap - matches text mode */
#define CHAR_WIDTH_LOOSE    10  /* 2 pixel gap - more readable */
#define CHAR_HEIGHT         16  /* VGA font height */
#define LINE_SPACING        18  /* Add 2 pixels between lines */

/* Special value for transparent background */
#define COLOR_TRANSPARENT   0xFF

void set_mode_12h(void);
void set_mode_03h(void);
void graphics_demo(void);
unsigned char read_pixel(int x, int y);
void set_pixel(int x, int y, unsigned char color);
void draw_rectangle(int x, int y, int width, int height, unsigned char color);
void draw_line(int x0, int y0, int x1, int y1, unsigned char color);
void draw_rectangle_outline(int x, int y, int width, int height, unsigned char color);
void draw_circle(int cx, int cy, int radius, unsigned char color);
void draw_char_from_bios_font(int x, int y, unsigned char c, unsigned char color);
void draw_char_extended(int x, int y, unsigned char c, unsigned char fg, unsigned char bg, int char_spacing);
void draw_string(int x, int y, const char *str, unsigned char color);
void draw_text_spaced(int x, int y, const char *text, unsigned char fg, unsigned char bg, int char_spacing);

/* 6x8 font functions */
void draw_char_6x8(int x, int y, unsigned char c, unsigned char fg, unsigned char bg);
void draw_string_6x8(int x, int y, const char *str, unsigned char fg, unsigned char bg);
void draw_text_centered(int y, const char *text, unsigned char fg, unsigned char bg);
void draw_text_right_aligned(int right_x, int y, const char *text, unsigned char fg, unsigned char bg);
void text_pos_to_pixels(int col, int row, int *x, int *y);
int get_text_width(const char *str);
void clear_graphics_screen(unsigned char color);
void save_vga_font(void);
void restore_vga_font(void);
void restore_dac_palette(void);
void set_aquinas_palette(void);

/* Mouse cursor functions */
void init_mouse_cursor(void);
void show_mouse_cursor(void);
void hide_mouse_cursor(void);
void update_mouse_cursor(int new_x, int new_y);
void get_mouse_cursor_pos(int *x, int *y);
void handle_graphics_mouse_move(int text_x, int text_y);
void handle_graphics_mouse_raw(signed char dx, signed char dy);

/* Graphics mode state */
extern int graphics_mode_active;

/* Access to saved VGA font */
extern unsigned char* get_saved_font(void);

#endif