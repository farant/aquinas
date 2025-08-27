/* DISPI Graphics Mode Initialization Implementation */

#include "dispi_init.h"
#include "dispi.h"
#include "dispi_cursor.h"
#include "grid.h"
#include "serial.h"
#include "vga.h"
#include "graphics.h"
#include "mouse.h"

/* From graphics.c - VGA state functions */
void save_vga_font(void);
void restore_vga_font(void);
void restore_dac_palette(void);

/* Get the standard Aquinas palette */
void dispi_get_aquinas_palette(unsigned char palette[16][3]) {
    /* Grayscale (0-5) */
    palette[0][0] = 0x00; palette[0][1] = 0x00; palette[0][2] = 0x00;  /* Black */
    palette[1][0] = 0x40; palette[1][1] = 0x40; palette[1][2] = 0x40;  /* Dark gray */
    palette[2][0] = 0x80; palette[2][1] = 0x80; palette[2][2] = 0x80;  /* Medium dark gray */
    palette[3][0] = 0xC0; palette[3][1] = 0xC0; palette[3][2] = 0xC0;  /* Medium gray */
    palette[4][0] = 0xE0; palette[4][1] = 0xE0; palette[4][2] = 0xE0;  /* Light gray */
    palette[5][0] = 0xFC; palette[5][1] = 0xFC; palette[5][2] = 0xFC;  /* White */
    
    /* Reds (6-8) */
    palette[6][0] = 0x80; palette[6][1] = 0x20; palette[6][2] = 0x20;  /* Dark red */
    palette[7][0] = 0xC0; palette[7][1] = 0x30; palette[7][2] = 0x30;  /* Medium red */
    palette[8][0] = 0xFC; palette[8][1] = 0x40; palette[8][2] = 0x40;  /* Bright red */
    
    /* Golds (9-11) */
    palette[9][0] = 0xA0; palette[9][1] = 0x80; palette[9][2] = 0x20;   /* Dark gold */
    palette[10][0] = 0xE0; palette[10][1] = 0xC0; palette[10][2] = 0x40; /* Medium gold */
    palette[11][0] = 0xFC; palette[11][1] = 0xE0; palette[11][2] = 0x60; /* Bright yellow-gold */
    
    /* Cyans (12-14) */
    palette[12][0] = 0x20; palette[12][1] = 0x80; palette[12][2] = 0xA0; /* Dark cyan */
    palette[13][0] = 0x40; palette[13][1] = 0xC0; palette[13][2] = 0xE0; /* Medium cyan */
    palette[14][0] = 0x60; palette[14][1] = 0xE0; palette[14][2] = 0xFC; /* Bright cyan */
    
    /* Background (15) */
    palette[15][0] = 0xB0; palette[15][1] = 0xA0; palette[15][2] = 0x80; /* Warm gray */
}

/* Initialize DISPI graphics mode with standard Aquinas palette */
DisplayDriver* dispi_graphics_init(void) {
    DisplayDriver *driver;
    unsigned char aquinas_palette[16][3];
    
    serial_write_string("Initializing DISPI graphics mode\n");
    
    /* Initialize grid system (required for DISPI) */
    grid_init();
    
    /* Save VGA state before switching to graphics */
    serial_write_string("Saving VGA font...\n");
    save_vga_font();
    
    /* Get DISPI driver */
    serial_write_string("Getting DISPI driver...\n");
    driver = dispi_get_driver();
    if (!driver) {
        serial_write_string("ERROR: Failed to get DISPI driver\n");
        return 0;
    }
    
    serial_write_string("Setting driver as active...\n");
    display_set_driver(driver);
    
    /* Enable double buffering for smooth rendering */
    if (!dispi_init_double_buffer()) {
        serial_write_string("WARNING: Double buffering failed, using single buffer\n");
    }
    
    /* Set up Aquinas color palette */
    dispi_get_aquinas_palette(aquinas_palette);
    if (driver->set_palette) {
        driver->set_palette(aquinas_palette);
    }
    
    /* Initialize mouse if not already done */
    if (!mouse_is_initialized()) {
        mouse_init(320, 240);
    }
    
    /* Clear screen with warm gray background to actually enable graphics mode */
    display_clear(15);  /* Use color 15 (warm gray) for background */
    
    /* Initialize and show mouse cursor for DISPI mode */
    dispi_cursor_init();
    dispi_cursor_show();
    
    /* Flip buffers if double buffering is enabled to show initial clear */
    if (dispi_is_double_buffered()) {
        dispi_flip_buffers();
    }
    
    serial_write_string("DISPI graphics initialization complete\n");
    
    return driver;
}

/* Cleanup and return to text mode */
void dispi_graphics_cleanup(GraphicsContext *gc) {
    serial_write_string("Cleaning up DISPI graphics mode\n");
    
    /* Hide mouse cursor before switching modes */
    dispi_cursor_hide();
    
    /* Destroy graphics context if provided */
    if (gc) {
        gc_destroy(gc);
    }
    
    /* Cleanup double buffering if enabled */
    if (dispi_is_double_buffered()) {
        dispi_cleanup_double_buffer();
    }
    
    /* Return to text mode */
    dispi_disable();
    restore_dac_palette();
    set_mode_03h();
    restore_vga_font();
    vga_clear_screen();
    
    serial_write_string("Returned to text mode\n");
}