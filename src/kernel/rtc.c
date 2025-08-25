/* RTC (Real-Time Clock) implementation
 * 
 * Reads the CMOS RTC to get actual date/time at boot
 * The RTC stores values in BCD (Binary Coded Decimal) format
 * where each nibble represents a decimal digit
 */

#include "rtc.h"
#include "io.h"
#include "timer.h"
#include "serial.h"

/* CMOS/RTC I/O ports */
#define CMOS_ADDRESS 0x70
#define CMOS_DATA    0x71

/* RTC register addresses */
#define RTC_SECONDS  0x00
#define RTC_MINUTES  0x02
#define RTC_HOURS    0x04
#define RTC_DAY      0x07
#define RTC_MONTH    0x08
#define RTC_YEAR     0x09
#define RTC_CENTURY  0x32
#define RTC_STATUS_A 0x0A
#define RTC_STATUS_B 0x0B

/* Store boot time */
static rtc_time_t boot_time;
static unsigned int boot_ticks;

/* Convert BCD to binary */
static unsigned char bcd_to_bin(unsigned char bcd) {
    return ((bcd >> 4) * 10) + (bcd & 0x0F);
}

/* Read a CMOS register */
static unsigned char read_cmos(unsigned char reg) {
    outb(CMOS_ADDRESS, reg);
    return inb(CMOS_DATA);
}

/* Check if RTC update is in progress */
static int is_update_in_progress(void) {
    return read_cmos(RTC_STATUS_A) & 0x80;
}

/* Read RTC time */
static void read_rtc(rtc_time_t *time) {
    unsigned char century;
    unsigned char last_second;
    unsigned char last_minute;
    unsigned char last_hour;
    unsigned char last_day;
    unsigned char last_month;
    unsigned char last_year;
    unsigned char last_century;
    unsigned char status_b;
    
    /* Wait for any update to complete */
    while (is_update_in_progress());
    
    /* Read all time/date registers */
    time->second = read_cmos(RTC_SECONDS);
    time->minute = read_cmos(RTC_MINUTES);
    time->hour = read_cmos(RTC_HOURS);
    time->day = read_cmos(RTC_DAY);
    time->month = read_cmos(RTC_MONTH);
    time->year = read_cmos(RTC_YEAR);
    century = read_cmos(RTC_CENTURY);
    
    /* Read registers again to ensure consistency */
    do {
        last_second = time->second;
        last_minute = time->minute;
        last_hour = time->hour;
        last_day = time->day;
        last_month = time->month;
        last_year = time->year;
        last_century = century;
        
        while (is_update_in_progress());
        
        time->second = read_cmos(RTC_SECONDS);
        time->minute = read_cmos(RTC_MINUTES);
        time->hour = read_cmos(RTC_HOURS);
        time->day = read_cmos(RTC_DAY);
        time->month = read_cmos(RTC_MONTH);
        time->year = read_cmos(RTC_YEAR);
        century = read_cmos(RTC_CENTURY);
    } while ((last_second != time->second) || (last_minute != time->minute) ||
             (last_hour != time->hour) || (last_day != time->day) ||
             (last_month != time->month) || (last_year != time->year) ||
             (last_century != century));
    
    /* Check if values are in BCD format */
    status_b = read_cmos(RTC_STATUS_B);
    if (!(status_b & 0x04)) {
        /* Convert from BCD to binary */
        time->second = bcd_to_bin(time->second);
        time->minute = bcd_to_bin(time->minute);
        time->hour = bcd_to_bin(time->hour & 0x7F);  /* Mask AM/PM bit */
        time->day = bcd_to_bin(time->day);
        time->month = bcd_to_bin(time->month);
        time->year = bcd_to_bin(time->year);
        century = bcd_to_bin(century);
    }
    
    /* Convert to full year */
    if (century == 0) {
        /* Century register might not be available, assume 21st century */
        century = 20;
    }
    time->year = (century * 100) + time->year;
    
    /* Handle 12-hour format if necessary */
    if (!(status_b & 0x02) && (time->hour & 0x80)) {
        /* 12-hour format with PM flag */
        time->hour = ((time->hour & 0x7F) + 12) % 24;
    }
}

/* Initialize RTC and read boot time */
void init_rtc(void) {
    /* Read current RTC time */
    read_rtc(&boot_time);
    
    /* Store current tick count */
    boot_ticks = get_ticks();
    
    /* Log boot time */
    serial_write_string("RTC: Boot time is ");
    serial_write_int(boot_time.year);
    serial_write_string("-");
    if (boot_time.month < 10) serial_write_string("0");
    serial_write_int(boot_time.month);
    serial_write_string("-");
    if (boot_time.day < 10) serial_write_string("0");
    serial_write_int(boot_time.day);
    serial_write_string(" ");
    if (boot_time.hour < 10) serial_write_string("0");
    serial_write_int(boot_time.hour);
    serial_write_string(":");
    if (boot_time.minute < 10) serial_write_string("0");
    serial_write_int(boot_time.minute);
    serial_write_string(":");
    if (boot_time.second < 10) serial_write_string("0");
    serial_write_int(boot_time.second);
    serial_write_string("\n");
}

/* Get boot time */
void get_boot_time(rtc_time_t *time) {
    *time = boot_time;
}

/* Time zone offset in hours (negative to subtract) */
#define TIMEZONE_OFFSET_HOURS -4  /* Subtract 4 hours from system time */

/* Get current time (boot time + elapsed) */
void get_current_time(rtc_time_t *time) {
    unsigned int elapsed_seconds;
    static const unsigned int days_in_month[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    int adjusted_hour;

    /* Start with boot time - copy field by field to avoid struct copy issues */
    time->second = boot_time.second;
    time->minute = boot_time.minute;
    time->hour = boot_time.hour;
    time->day = boot_time.day;
    time->month = boot_time.month;
    time->year = boot_time.year;
    
    /* Apply timezone offset */
    adjusted_hour = (int)time->hour + TIMEZONE_OFFSET_HOURS;
    if (adjusted_hour < 0) {
        /* Went to previous day */
        adjusted_hour += 24;
        time->day--;
        if (time->day < 1) {
            /* Went to previous month */
            time->month--;
            if (time->month < 1) {
                /* Went to previous year */
                time->month = 12;
                time->year--;
            }
            /* Set day to last day of previous month */
            time->day = days_in_month[time->month - 1];
            /* Handle leap year for February */
            if (time->month == 2 && ((time->year % 4 == 0 && time->year % 100 != 0) || (time->year % 400 == 0))) {
                time->day = 29;
            }
        }
    } else if (adjusted_hour >= 24) {
        /* Went to next day */
        adjusted_hour -= 24;
        time->day++;
        /* Check if we went to next month */
        if (time->day > days_in_month[time->month - 1]) {
            /* Handle leap year for February */
            if (!(time->month == 2 && time->day == 29 && 
                  ((time->year % 4 == 0 && time->year % 100 != 0) || (time->year % 400 == 0)))) {
                time->day = 1;
                time->month++;
                if (time->month > 12) {
                    time->month = 1;
                    time->year++;
                }
            }
        }
    }
    time->hour = (unsigned char)adjusted_hour;
    
    /* Calculate elapsed seconds since boot */
    elapsed_seconds = (get_ticks() - boot_ticks) / 1000;
    
    /* Add elapsed time to boot time */
    time->second += elapsed_seconds % 60;
    elapsed_seconds /= 60;
    time->minute += elapsed_seconds % 60;
    elapsed_seconds /= 60;
    time->hour += elapsed_seconds % 24;
    elapsed_seconds /= 24;
    
    /* Handle overflow */
    if (time->second >= 60) {
        time->minute += time->second / 60;
        time->second %= 60;
    }
    if (time->minute >= 60) {
        time->hour += time->minute / 60;
        time->minute %= 60;
    }
    if (time->hour >= 24) {
        time->day += time->hour / 24;
        time->hour %= 24;
    }
    
    /* Handle month/year overflow (simplified - doesn't handle all edge cases) */
    /* Bounds check before accessing array */
    if (time->month >= 1 && time->month <= 12 && time->day > 0) {
        while (time->day > days_in_month[time->month - 1]) {
            time->day -= days_in_month[time->month - 1];
            time->month++;
            if (time->month > 12) {
                time->month = 1;
                time->year++;
            }
        }
    }
}

/* Simple conversion to seconds (not a real epoch, just for comparison) */
unsigned int time_to_seconds(rtc_time_t *time) {
    /* Simplified: just convert current time to seconds of the day */
    return (time->hour * 3600) + (time->minute * 60) + time->second;
}
