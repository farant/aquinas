/* Keyboard driver for AquinasOS */

#include "keyboard.h"
#include "io.h"
#include "vga.h"

/* US QWERTY keyboard scancode to ASCII mapping */
static const char scancode_to_ascii[128] = {
    0,  27, '1', '2', '3', '4', '5', '6', '7', '8',  /* 0-9 */
    '9', '0', '-', '=', '\b',  /* Backspace */
    '\t',  /* Tab */
    'q', 'w', 'e', 'r',  /* 16-19 */
    't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',  /* Enter */
    0,  /* Control */
    'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';',  /* 30-39 */
    '\'', '`',   0,  /* Left shift */
    '\\', 'z', 'x', 'c', 'v', 'b', 'n',  /* 40-49 */
    'm', ',', '.', '/',   0,  /* Right shift */
    '*',
    0,  /* Alt */
    ' ',  /* Space bar */
    0,  /* Caps lock */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* F1-F10 */
    0,  /* Num lock*/
    0,  /* Scroll Lock */
    0,  /* Home */
    0,  /* Up Arrow */
    0,  /* Page Up */
    '-',
    0,  /* Left Arrow */
    0,
    0,  /* Right Arrow */
    '+',
    0,  /* End */
    0,  /* Down Arrow */
    0,  /* Page Down */
    0,  /* Insert */
    0,  /* Delete */
    0, 0, 0,
    0,  /* F11 */
    0,  /* F12 */
    0,  /* All other keys are undefined */
};

/* Shift key mappings for special characters */
static const char scancode_to_ascii_shift[128] = {
    0,  27, '!', '@', '#', '$', '%', '^', '&', '*',
    '(', ')', '_', '+', '\b',
    '\t',
    'Q', 'W', 'E', 'R',
    'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
    0,
    'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':',
    '"', '~',   0,
    '|', 'Z', 'X', 'C', 'V', 'B', 'N',
    'M', '<', '>', '?',   0,
    '*',
    0,
    ' ',
    0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, '-', 0, 0, 0, '+',
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

static int shift_pressed = 0;
static int ctrl_pressed = 0;
static int alt_pressed = 0;

/* Wait for keyboard data to be available */
static void keyboard_wait_data(void) {
    int timeout = 100000;
    while (--timeout) {
        if (inb(KEYBOARD_STATUS_PORT) & 1) {
            return;
        }
    }
}

/* Read a scancode from the keyboard */
unsigned char keyboard_read_scancode(void) {
    keyboard_wait_data();
    return inb(KEYBOARD_DATA_PORT);
}

/* Get a character from the keyboard (blocking) */
char getchar(void) {
    unsigned char scancode;
    char ascii;
    
    while (1) {
        scancode = keyboard_read_scancode();
        
        /* Handle key release (scancode with bit 7 set) */
        if (scancode & 0x80) {
            scancode &= 0x7F;  /* Clear bit 7 */
            
            /* Track modifier key releases */
            if (scancode == 0x2A || scancode == 0x36) {  /* Shift */
                shift_pressed = 0;
            } else if (scancode == 0x1D) {  /* Ctrl */
                ctrl_pressed = 0;
            } else if (scancode == 0x38) {  /* Alt */
                alt_pressed = 0;
            }
            continue;
        }
        
        /* Handle key press */
        /* Track modifier key presses */
        if (scancode == 0x2A || scancode == 0x36) {  /* Shift */
            shift_pressed = 1;
            continue;
        } else if (scancode == 0x1D) {  /* Ctrl */
            ctrl_pressed = 1;
            continue;
        } else if (scancode == 0x38) {  /* Alt */
            alt_pressed = 1;
            continue;
        }
        
        /* Convert scancode to ASCII */
        if (shift_pressed && scancode < 128) {
            ascii = scancode_to_ascii_shift[scancode];
        } else if (scancode < 128) {
            ascii = scancode_to_ascii[scancode];
        } else {
            continue;
        }
        
        /* Return valid ASCII character */
        if (ascii != 0) {
            return ascii;
        }
    }
}