#ifndef DISPI_DEMO_H
#define DISPI_DEMO_H

/* DISPI Graphics Demo
 * Interactive demonstration of DISPI graphics capabilities
 */

/* Main demo function */
void test_dispi_driver(void);

/* Text rendering functions using 6x8 font 
 * These are reusable and should be moved to dispi.c
 */
void dispi_draw_char(int x, int y, unsigned char c, unsigned char fg, unsigned char bg);
void dispi_draw_string(int x, int y, const char *str, unsigned char fg, unsigned char bg);

#endif