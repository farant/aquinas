#include "serial.h"
#include "io.h"

/* Initialize serial port for mouse (COM1) */
void init_serial_port(void) {
    /* Disable interrupts */
    outb(COM1_IER, 0x00);
    
    /* Set baud rate to 1200 (divisor = 96) for serial mouse */
    outb(COM1_LCR, 0x80);  /* Enable DLAB */
    outb(COM1_DATA, 0x60); /* Set divisor low byte (96) */
    outb(COM1_IER, 0x00);  /* Set divisor high byte (0) */
    
    /* 7 data bits, 1 stop bit, no parity (Microsoft mouse protocol) */
    outb(COM1_LCR, 0x02);
    
    /* Enable FIFO */
    outb(COM1_FCR, 0xC7);
    
    /* Enable DTR/RTS to power the mouse */
    outb(COM1_MCR, 0x03);
}

/* Initialize COM2 for debug output */
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
            serial_write_char('\r');  /* Add carriage return before newline */
        }
        serial_write_char(*str);
        str++;
    }
}

/* Write a hex number to COM2 (for debugging) */
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