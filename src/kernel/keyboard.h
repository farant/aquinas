#ifndef KEYBOARD_H
#define KEYBOARD_H

/* Keyboard ports */
#define KEYBOARD_DATA_PORT   0x60
#define KEYBOARD_STATUS_PORT 0x64

/* Keyboard functions */
unsigned char keyboard_read_scancode(void);
char getchar(void);

#endif