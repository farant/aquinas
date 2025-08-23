/* VGA Text Mode Display Implementation */

#include "vga.h"
#include "io.h"

/* Initialize VGA display to a clean state */
void vga_init(void) {
    vga_clear_screen();
    vga_hide_cursor();  /* Start with cursor hidden */
}

/* Clear the entire screen with default color */
void vga_clear_screen(void) {
    int i;
    /* Fill entire buffer with spaces and default color */
    for (i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) {
        VGA_BUFFER[i] = VGA_COLOR | ' ';
    }
}

/* Write a character at specific position with color.
 * Why bounds checking: Prevents writing outside VGA buffer which could
 * corrupt other memory or cause crashes. */
void vga_write_char(int pos, char c, unsigned short color) {
    if (!SAFE_VGA_POS(pos)) return;
    VGA_BUFFER[pos] = color | c;
}

/* Write a string starting at position */
void vga_write_string(int pos, const char *str, unsigned short color) {
    if (!str) return;
    
    while (*str && SAFE_VGA_POS(pos)) {
        VGA_BUFFER[pos] = color | *str;
        str++;
        pos++;
    }
}

/* Fill a region with a specific character and color */
void vga_fill_region(int start, int length, char c, unsigned short color) {
    int i;
    for (i = 0; i < length && SAFE_VGA_POS(start + i); i++) {
        VGA_BUFFER[start + i] = color | c;
    }
}

/* Update hardware cursor position.
 * The VGA hardware cursor is controlled through I/O ports.
 * We write the position as two bytes (high and low) to the VGA registers. */
void vga_set_cursor(int pos) {
    if (!SAFE_VGA_POS(pos)) {
        vga_hide_cursor();
        return;
    }
    
    /* Tell VGA we're setting cursor high byte */
    outb(VGA_CTRL_REGISTER, VGA_CURSOR_HIGH);
    outb(VGA_DATA_REGISTER, (pos >> 8) & 0xFF);
    
    /* Tell VGA we're setting cursor low byte */
    outb(VGA_CTRL_REGISTER, VGA_CURSOR_LOW);
    outb(VGA_DATA_REGISTER, pos & 0xFF);
}

/* Disable hardware cursor by moving it off-screen */
void vga_hide_cursor(void) {
    /* Position 0x2000 is well beyond the visible screen */
    outb(VGA_CTRL_REGISTER, VGA_CURSOR_HIGH);
    outb(VGA_DATA_REGISTER, 0x20);
    outb(VGA_CTRL_REGISTER, VGA_CURSOR_LOW);
    outb(VGA_DATA_REGISTER, 0x00);
}

/* Get character at position (without color) */
char vga_get_char(int pos) {
    if (!SAFE_VGA_POS(pos)) return '\0';
    return VGA_BUFFER[pos] & 0xFF;
}

/* Get full 16-bit value (char + color) at position */
unsigned short vga_get_entry(int pos) {
    if (!SAFE_VGA_POS(pos)) return 0;
    return VGA_BUFFER[pos];
}