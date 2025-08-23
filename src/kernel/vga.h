#ifndef VGA_H
#define VGA_H

#define VGA_BUFFER ((unsigned short*)0xB8000)
#define VGA_WIDTH 80
#define VGA_HEIGHT 25
#define VGA_COLOR 0x1F00  /* Blue background, white text */

/* VGA functions */
void putchar(char c);
void puts(const char* str);
void clear_screen(void);

/* Global cursor position */
extern int cursor_pos;

#endif