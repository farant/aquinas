#ifndef SERIAL_H
#define SERIAL_H

/* Serial port definitions */
/* COM1 - Used for mouse */
#define COM1 0x3F8
#define COM1_DATA (COM1 + 0)
#define COM1_IER  (COM1 + 1)  /* Interrupt Enable Register */
#define COM1_FCR  (COM1 + 2)  /* FIFO Control Register */
#define COM1_LCR  (COM1 + 3)  /* Line Control Register */
#define COM1_MCR  (COM1 + 4)  /* Modem Control Register */
#define COM1_LSR  (COM1 + 5)  /* Line Status Register */

/* COM2 - Used for debug output */
#define COM2 0x2F8
#define COM2_DATA (COM2 + 0)
#define COM2_IER  (COM2 + 1)
#define COM2_FCR  (COM2 + 2)
#define COM2_LCR  (COM2 + 3)
#define COM2_MCR  (COM2 + 4)
#define COM2_LSR  (COM2 + 5)

/* Serial functions */
void init_serial_port(void);      /* Initialize COM1 for mouse */
void init_debug_serial(void);     /* Initialize COM2 for debug */
int serial_transmit_empty(void);
void serial_write_char(char c);
void serial_write_string(const char *str);
void serial_write_hex(unsigned int value);

#endif /* SERIAL_H */