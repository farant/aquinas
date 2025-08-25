/* Serial Port Communication Module
 *
 * DESIGN
 * ------
 * This module handles serial port communication for both mouse input
 * and debug output. We use two separate COM ports to avoid conflicts:
 * 
 * - COM1 (0x3F8): Mouse input using Microsoft Serial Mouse protocol
 * - COM2 (0x2F8): Debug output for development and troubleshooting
 * 
 * The separation ensures that debug messages don't interfere with mouse
 * packets, which would cause erratic cursor behavior. Both ports are
 * configured differently to match their specific use cases.
 */

#include "serial.h"
#include "io.h"

/* Initialize serial port for mouse (COM1).
 * Configures for Microsoft Serial Mouse: 1200 baud, 7N1.
 * Returns: void (initialization always succeeds on standard hardware) */
void init_serial_port(void) {
    /* Disable interrupts */
    outb(COM1_IER, 0x00);
    
    /* Set baud rate to 1200 (divisor = 96) for serial mouse.
     * Why 1200 baud: This is the standard for Microsoft serial mice.
     * Higher rates would work but provide no benefit since mice only
     * generate ~40 packets/second at most during fast movement. */
    outb(COM1_LCR, 0x80);  /* Enable DLAB (Divisor Latch Access Bit) */
    outb(COM1_DATA, 0x60); /* Set divisor low byte (96) */
    outb(COM1_IER, 0x00);  /* Set divisor high byte (0) */
    
    /* 7 data bits, 1 stop bit, no parity (Microsoft mouse protocol) */
    outb(COM1_LCR, 0x02);
    
    /* Enable FIFO */
    outb(COM1_FCR, 0xC7);
    
    /* Enable DTR/RTS to power the mouse */
    outb(COM1_MCR, 0x03);
}

/* Initialize COM2 for debug output.
 * Configures for high-speed debug: 115200 baud, 8N1.
 * This allows real-time debug messages without slowing down the system. */
void init_debug_serial(void) {
    /* Disable interrupts */
    outb(COM2_IER, 0x00);
    
    /* Set baud rate to 115200 (divisor = 1) for fast debug output */
    outb(COM2_LCR, 0x80);  /* Enable DLAB */
    outb(COM2_DATA, 0x01); /* Set divisor low byte */
    outb(COM2_IER, 0x00);  /* Set divisor high byte */
    
    /* 8 data bits, 1 stop bit, no parity */
    outb(COM2_LCR, 0x03);
    
    /* Enable FIFO */
    outb(COM2_FCR, 0xC7);
    
    /* Enable DTR/RTS */
    outb(COM2_MCR, 0x03);
}

/* Check if COM2 transmit buffer is empty */
int serial_transmit_empty(void) {
    return inb(COM2_LSR) & 0x20;
}

/* Write a character to COM2 (debug port) */
void serial_write_char(char c) {
    while (serial_transmit_empty() == 0);
    outb(COM2_DATA, c);
}

/* Write a string to COM2 (debug port) */
void serial_write_string(const char *str) {
    while (*str) {
        if (*str == '\n') {
            /* Add carriage return before newline.
             * Why: Terminal emulators expect CRLF line endings in raw mode.
             * Without the CR, lines would start at the current column position. */
            serial_write_char('\r');
        }
        serial_write_char(*str);
        str++;
    }
}

/* Write a hex number to COM2 (for debugging).
 * Formats as "0x" followed by 8 hex digits (32-bit value).
 * Always shows all 8 digits for consistent alignment in logs. */
void serial_write_hex(unsigned int value) {
    char buffer[9];  /* 8 hex digits + null terminator */
    const char *hex = "0123456789ABCDEF";
    int i;
    
    for (i = 7; i >= 0; i--) {
        buffer[i] = hex[value & 0xF];
        value >>= 4;
    }
    buffer[8] = '\0';
    
    serial_write_string("0x");
    serial_write_string(buffer);
}

/* Write a decimal integer to COM2 (for debugging).
 * Handles negative numbers by printing minus sign.
 * Converts integer to decimal string representation. */
void serial_write_int(int value) {
    char buffer[12];  /* Max int is -2147483648 (11 chars + null) */
    int i = 0;
    int is_negative = 0;
    unsigned int abs_value;
    
    /* Handle negative numbers */
    if (value < 0) {
        is_negative = 1;
        /* Handle INT_MIN special case (can't negate it) */
        /* Use unsigned literal with cast to avoid C90 warning */
        if (value == (int)0x80000000) {
            serial_write_string("-2147483648");
            return;
        }
        abs_value = -value;
    } else {
        abs_value = value;
    }
    
    /* Handle zero special case */
    if (abs_value == 0) {
        serial_write_char('0');
        return;
    }
    
    /* Convert to string (backwards) */
    while (abs_value > 0) {
        buffer[i++] = '0' + (abs_value % 10);
        abs_value /= 10;
    }
    
    /* Add minus sign if negative */
    if (is_negative) {
        serial_write_char('-');
    }
    
    /* Print digits in reverse order */
    while (i > 0) {
        serial_write_char(buffer[--i]);
    }
}