/* Timer and interrupt handling for AquinasOS
 * 
 * Provides millisecond-precision timing using the PIT (Programmable Interval Timer)
 * The PIT is configured to generate interrupts at 1000Hz (every 1ms)
 * This gives us a system tick counter that tracks milliseconds since boot
 */

#ifndef TIMER_H
#define TIMER_H

/* Initialize the timer system (PIT and interrupts) */
void init_timer(void);

/* Get current system ticks (milliseconds since boot) */
unsigned int get_ticks(void);

/* Get elapsed milliseconds since a previous tick count */
unsigned int get_elapsed_ms(unsigned int start_ticks);

#endif