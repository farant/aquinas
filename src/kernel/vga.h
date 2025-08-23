/* VGA Text Mode Display Module
 *
 * DESIGN
 * ------
 * This module handles all VGA text mode operations including:
 * - Direct VGA buffer manipulation at 0xB8000
 * - Hardware cursor control through VGA registers
 * - Color attribute management
 * - Screen clearing and character display
 * 
 * The VGA buffer is a linear array of 16-bit values where:
 * - Low byte: ASCII character
 * - High byte: Color attributes (background | foreground)
 */

#ifndef VGA_H
#define VGA_H

/* VGA dimensions and memory location */
#define VGA_BUFFER ((unsigned short*)0xB8000)
#define VGA_WIDTH 80
#define VGA_HEIGHT 25

/* Color codes (high byte of VGA buffer entries) */
#define VGA_COLOR 0x1F00  /* Blue background, white text (default) */
#define VGA_COLOR_NAV_BAR 0x7000  /* Gray background, black text */
#define VGA_COLOR_MOUSE   0x2F00  /* Green background, white text */
#define VGA_COLOR_HIGHLIGHT 0x4F00 /* Red background, white text */

/* VGA hardware cursor control ports */
#define VGA_CTRL_REGISTER 0x3D4
#define VGA_DATA_REGISTER 0x3D5
#define VGA_CURSOR_HIGH 0x0E
#define VGA_CURSOR_LOW 0x0F

/* Bounds checking macro for safe buffer access */
#define SAFE_VGA_POS(pos) ((pos) >= 0 && (pos) < (VGA_WIDTH * VGA_HEIGHT))
#define SAFE_BUFFER_ACCESS(buf, pos, len) ((pos) >= 0 && (pos) < (len))

/* Initialize VGA display */
void vga_init(void);

/* Clear the entire screen with default color */
void vga_clear_screen(void);

/* Write a character at specific position with color */
void vga_write_char(int pos, char c, unsigned short color);

/* Write a string starting at position */
void vga_write_string(int pos, const char *str, unsigned short color);

/* Fill a region with a specific character and color */
void vga_fill_region(int start, int length, char c, unsigned short color);

/* Update hardware cursor position */
void vga_set_cursor(int pos);

/* Disable hardware cursor (set position off-screen) */
void vga_hide_cursor(void);

/* Get character at position (without color) */
char vga_get_char(int pos);

/* Get full 16-bit value (char + color) at position */
unsigned short vga_get_entry(int pos);

#endif /* VGA_H */