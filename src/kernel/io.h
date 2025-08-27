#ifndef IO_H
#define IO_H

/* Read a byte from a port */
static unsigned char inb(unsigned short port) {
    unsigned char ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "dN"(port));
    return ret;
}

/* Write a byte to a port */
static void outb(unsigned short port, unsigned char val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "dN"(port));
}

/* Read a word from a port */
static unsigned short inw(unsigned short port) {
    unsigned short ret;
    __asm__ volatile ("inw %1, %0" : "=a"(ret) : "dN"(port));
    return ret;
}

/* Write a word to a port */
static void outw(unsigned short port, unsigned short val) {
    __asm__ volatile ("outw %0, %1" : : "a"(val), "dN"(port));
}

/* Read a dword from a port */
static unsigned int inl(unsigned short port) {
    unsigned int ret;
    __asm__ volatile ("inl %1, %0" : "=a"(ret) : "dN"(port));
    return ret;
}

/* Write a dword to a port */
static void outl(unsigned short port, unsigned int val) {
    __asm__ volatile ("outl %0, %1" : : "a"(val), "dN"(port));
}

/* Aliases for compatibility */
#define port_byte_in(port) inb(port)
#define port_byte_out(port, val) outb(port, val)
#define port_word_in(port) inw(port)
#define port_word_out(port, val) outw(port, val)

#endif