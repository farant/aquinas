/* Real-Time Clock (RTC) interface for reading system date/time
 * 
 * The RTC maintains the current date and time using battery backup
 * We read it once at boot to get the current time, then use our
 * timer interrupts to track elapsed time from that point
 */

#ifndef RTC_H
#define RTC_H

/* Time structure */
typedef struct {
    unsigned char second;   /* 0-59 */
    unsigned char minute;   /* 0-59 */
    unsigned char hour;     /* 0-23 */
    unsigned char day;      /* 1-31 */
    unsigned char month;    /* 1-12 */
    unsigned int year;      /* Full year (e.g., 2025) */
} rtc_time_t;

/* Initialize RTC and read current time */
void init_rtc(void);

/* Get the boot time (when RTC was initialized) */
void get_boot_time(rtc_time_t *time);

/* Get current time (boot time + elapsed) */
void get_current_time(rtc_time_t *time);

/* Convert time to seconds since epoch (simplified) */
unsigned int time_to_seconds(rtc_time_t *time);

#endif