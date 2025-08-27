/* DISPI Mouse Cursor Implementation */

#include "dispi_cursor.h"
#include "display_driver.h"
#include "serial.h"

/* Classic arrow cursor bitmap - 12x20 pixels
 * Each row is represented as 12 bits packed into bytes */
static const unsigned char cursor_arrow[] = {
    0x80, 0x00,  /* X....... ........ */
    0xC0, 0x00,  /* XX...... ........ */
    0xE0, 0x00,  /* XXX..... ........ */
    0xF0, 0x00,  /* XXXX.... ........ */
    0xF8, 0x00,  /* XXXXX... ........ */
    0xFC, 0x00,  /* XXXXXX.. ........ */
    0xFE, 0x00,  /* XXXXXXX. ........ */
    0xFF, 0x00,  /* XXXXXXXX ........ */
    0xFF, 0x80,  /* XXXXXXXX X....... */
    0xFF, 0xC0,  /* XXXXXXXX XX...... */
    0xFF, 0xE0,  /* XXXXXXXX XXX..... */
    0xFC, 0x00,  /* XXXXXX.. ........ */
    0xEE, 0x00,  /* XXX.XXX. ........ */
    0xE7, 0x00,  /* XXX..XXX ........ */
    0xC3, 0x00,  /* XX....XX ........ */
    0xC3, 0x80,  /* XX....XX X....... */
    0x81, 0x80,  /* X......X X....... */
    0x81, 0xC0,  /* X......X XX...... */
    0x00, 0xC0,  /* ........ XX...... */
    0x00, 0xC0   /* ........ XX...... */
};

/* Cursor state */
static struct {
    int x, y;                               /* Current position */
    int saved_x, saved_y;                   /* Position where background was saved */
    int visible;                            /* Is cursor shown? */
    unsigned char saved_bg[CURSOR_BG_SIZE]; /* Saved background pixels */
} cursor_state = {
    320, 240,    /* Start in center of 640x480 */
    -1, -1,      /* No saved position initially */
    0,           /* Hidden initially */
    {0}          /* Clear background buffer */
};

/* Save background pixels where cursor will be drawn */
static void save_background(int x, int y) {
    DisplayDriver *driver = display_get_driver();
    int row, col, px, py, bg_index = 0;
    
    if (!driver || !driver->get_pixel) {
        return;
    }
    
    cursor_state.saved_x = x;
    cursor_state.saved_y = y;
    
    /* Save pixels including 1-pixel border around cursor */
    for (row = -1; row <= CURSOR_HEIGHT; row++) {
        for (col = -1; col <= CURSOR_WIDTH; col++) {
            px = x + col - CURSOR_HOTSPOT_X;
            py = y + row - CURSOR_HOTSPOT_Y;
            
            /* Check bounds */
            if (px >= 0 && px < driver->width && py >= 0 && py < driver->height) {
                cursor_state.saved_bg[bg_index] = driver->get_pixel(px, py);
            } else {
                cursor_state.saved_bg[bg_index] = 0; /* Black for out of bounds */
            }
            bg_index++;
        }
    }
}

/* Restore background pixels where cursor was drawn */
static void restore_background(void) {
    DisplayDriver *driver = display_get_driver();
    int row, col, px, py, bg_index = 0;
    
    if (!driver || !driver->set_pixel) {
        return;
    }
    
    if (cursor_state.saved_x < 0 || cursor_state.saved_y < 0) {
        return; /* No saved background */
    }
    
    /* Restore pixels */
    for (row = -1; row <= CURSOR_HEIGHT; row++) {
        for (col = -1; col <= CURSOR_WIDTH; col++) {
            px = cursor_state.saved_x + col - CURSOR_HOTSPOT_X;
            py = cursor_state.saved_y + row - CURSOR_HOTSPOT_Y;
            
            /* Check bounds */
            if (px >= 0 && px < driver->width && py >= 0 && py < driver->height) {
                driver->set_pixel(px, py, cursor_state.saved_bg[bg_index]);
            }
            bg_index++;
        }
    }
}

/* Draw the cursor with black outline */
static void draw_cursor_at(int x, int y) {
    DisplayDriver *driver = display_get_driver();
    int row, col, px, py;
    int byte_index, bit_index;
    int dx, dy, n_row, n_col;
    
    if (!driver || !driver->set_pixel) {
        return;
    }
    
    /* First pass: Draw black outline */
    for (row = 0; row < CURSOR_HEIGHT; row++) {
        for (col = 0; col < CURSOR_WIDTH; col++) {
            byte_index = row * 2 + (col / 8);
            bit_index = 7 - (col % 8);
            
            /* Check if this pixel is part of cursor */
            if (cursor_arrow[byte_index] & (1 << bit_index)) {
                /* Check all 8 neighbors for outline */
                for (dy = -1; dy <= 1; dy++) {
                    for (dx = -1; dx <= 1; dx++) {
                        if (dx == 0 && dy == 0) continue;
                        
                        px = x + col + dx - CURSOR_HOTSPOT_X;
                        py = y + row + dy - CURSOR_HOTSPOT_Y;
                        
                        /* Check bounds */
                        if (px >= 0 && px < driver->width && py >= 0 && py < driver->height) {
                            /* Check if neighbor is outside cursor shape */
                            n_col = col + dx;
                            n_row = row + dy;
                            
                            if (n_col < 0 || n_col >= CURSOR_WIDTH || 
                                n_row < 0 || n_row >= CURSOR_HEIGHT) {
                                /* Neighbor is outside cursor bounds - draw outline */
                                driver->set_pixel(px, py, 0); /* Black outline */
                            } else {
                                /* Check if neighbor pixel is not part of cursor */
                                int n_byte = n_row * 2 + (n_col / 8);
                                int n_bit = 7 - (n_col % 8);
                                if (!(cursor_arrow[n_byte] & (1 << n_bit))) {
                                    driver->set_pixel(px, py, 0); /* Black outline */
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    
    /* Second pass: Draw white cursor body */
    for (row = 0; row < CURSOR_HEIGHT; row++) {
        for (col = 0; col < CURSOR_WIDTH; col++) {
            byte_index = row * 2 + (col / 8);
            bit_index = 7 - (col % 8);
            
            if (cursor_arrow[byte_index] & (1 << bit_index)) {
                px = x + col - CURSOR_HOTSPOT_X;
                py = y + row - CURSOR_HOTSPOT_Y;
                
                /* Check bounds */
                if (px >= 0 && px < driver->width && py >= 0 && py < driver->height) {
                    driver->set_pixel(px, py, 5); /* White cursor */
                }
            }
        }
    }
}

/* Initialize the cursor system */
void dispi_cursor_init(void) {
    cursor_state.x = 320;
    cursor_state.y = 240;
    cursor_state.saved_x = -1;
    cursor_state.saved_y = -1;
    cursor_state.visible = 0;
    
    serial_write_string("DISPI cursor initialized\n");
}

/* Show the cursor */
void dispi_cursor_show(void) {
    if (!cursor_state.visible) {
        save_background(cursor_state.x, cursor_state.y);
        draw_cursor_at(cursor_state.x, cursor_state.y);
        cursor_state.visible = 1;
    }
}

/* Hide the cursor */
void dispi_cursor_hide(void) {
    if (cursor_state.visible) {
        restore_background();
        cursor_state.visible = 0;
        cursor_state.saved_x = -1;
        cursor_state.saved_y = -1;
    }
}

/* Update cursor position */
void dispi_cursor_move(int x, int y) {
    DisplayDriver *driver = display_get_driver();
    
    if (!driver) {
        return;
    }
    
    /* Clamp to screen bounds */
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x >= driver->width) x = driver->width - 1;
    if (y >= driver->height) y = driver->height - 1;
    
    /* Skip if no movement */
    if (x == cursor_state.x && y == cursor_state.y) {
        return;
    }
    
    if (cursor_state.visible) {
        /* Restore old position */
        restore_background();
        
        /* Update position */
        cursor_state.x = x;
        cursor_state.y = y;
        
        /* Draw at new position */
        save_background(cursor_state.x, cursor_state.y);
        draw_cursor_at(cursor_state.x, cursor_state.y);
    } else {
        /* Just update position */
        cursor_state.x = x;
        cursor_state.y = y;
    }
}

/* Get current cursor position */
void dispi_cursor_get_pos(int *x, int *y) {
    if (x) *x = cursor_state.x;
    if (y) *y = cursor_state.y;
}

/* Check if cursor is visible */
int dispi_cursor_is_visible(void) {
    return cursor_state.visible;
}