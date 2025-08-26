#ifndef GRAPHICS_H
#define GRAPHICS_H

#define VGA_GRAPHICS_BUFFER 0xA0000
#define VGA_WIDTH_12H 640
#define VGA_HEIGHT_12H 480
#define VGA_PLANES 4

void set_mode_12h(void);
void set_mode_03h(void);
void graphics_demo(void);
void set_pixel(int x, int y, unsigned char color);
void draw_rectangle(int x, int y, int width, int height, unsigned char color);
void draw_line(int x0, int y0, int x1, int y1, unsigned char color);
void draw_rectangle_outline(int x, int y, int width, int height, unsigned char color);
void draw_circle(int cx, int cy, int radius, unsigned char color);
void draw_char_from_bios_font(int x, int y, unsigned char c, unsigned char color);
void draw_string(int x, int y, const char *str, unsigned char color);
void clear_graphics_screen(unsigned char color);
void save_vga_font(void);
void restore_vga_font(void);
void restore_dac_palette(void);
void set_aquinas_palette(void);

#endif