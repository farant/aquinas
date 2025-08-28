/* Display driver abstraction layer
 * 
 * This provides a common interface for different display devices,
 * allowing us to switch between VGA mode 12h and DISPI/VBE at runtime.
 */

#include "display_driver.h"
#include "serial.h"

/* The currently active display driver */
DisplayDriver *active_display_driver = 0;

/* Set the active display driver */
void display_set_driver(DisplayDriver *driver) {
    if (active_display_driver && active_display_driver->shutdown) {
        active_display_driver->shutdown();
    }
    
    active_display_driver = driver;
    
    if (active_display_driver) {
        serial_write_string("Display driver set: ");
        if (active_display_driver->name) {
            serial_write_string(active_display_driver->name);
        } else {
            serial_write_string("(null name)");
        }
        serial_write_string("\n");
        
        serial_write_string("Checking driver->init: ");
        serial_write_hex((unsigned int)active_display_driver->init);
        serial_write_string("\n");
        if (active_display_driver->init) {
            serial_write_string("Calling driver->init()\n");
            active_display_driver->init();
            serial_write_string("driver->init() returned\n");
        } else {
            serial_write_string("driver->init is NULL!\n");
        }
    }
}

/* Get the active display driver */
DisplayDriver *display_get_driver(void) {
    return active_display_driver;
}

/* Initialize the display */
void display_init(void) {
    if (active_display_driver && active_display_driver->init) {
        active_display_driver->init();
    }
}

/* Shutdown the display */
void display_shutdown(void) {
    if (active_display_driver && active_display_driver->shutdown) {
        active_display_driver->shutdown();
    }
}

/* Set a pixel */
void display_set_pixel(int x, int y, unsigned char color) {
    if (active_display_driver && active_display_driver->set_pixel) {
        active_display_driver->set_pixel(x, y, color);
    }
}

/* Get a pixel */
unsigned char display_get_pixel(int x, int y) {
    if (active_display_driver && active_display_driver->get_pixel) {
        return active_display_driver->get_pixel(x, y);
    }
    return 0;
}

/* Fill a rectangle */
void display_fill_rect(int x, int y, int w, int h, unsigned char color) {
    if (active_display_driver && active_display_driver->fill_rect) {
        active_display_driver->fill_rect(x, y, w, h, color);
    }
}

/* Blit a buffer to screen */
void display_blit(int x, int y, int w, int h, unsigned char *src, int src_stride) {
    if (active_display_driver && active_display_driver->blit) {
        active_display_driver->blit(x, y, w, h, src, src_stride);
    }
}

/* Clear the screen */
void display_clear(unsigned char color) {
    if (active_display_driver) {
        if (active_display_driver->clear_screen) {
            active_display_driver->clear_screen(color);
        } else if (active_display_driver->fill_rect) {
            /* Fallback to fill_rect if no clear_screen */
            active_display_driver->fill_rect(0, 0, 
                active_display_driver->width, 
                active_display_driver->height, 
                color);
        }
    }
}