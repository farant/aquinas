/* Mouse Driver
 * 
 * Centralized mouse handling for Aquinas OS.
 * Supports Microsoft Serial Mouse protocol on COM1.
 */

#include "mouse.h"
#include "io.h"
#include "serial.h"

/* Global mouse state */
static MouseState mouse_state = {
    320,    /* x */
    240,    /* y */
    0,      /* button_state */
    0,      /* prev_button_state */
    {0,0,0}, /* bytes */
    0,      /* byte_num */
    0,      /* callback (NULL) */
    0       /* initialized */
};

/* Initialize mouse driver */
void mouse_init(int initial_x, int initial_y) {
    mouse_state.x = initial_x;
    mouse_state.y = initial_y;
    mouse_state.button_state = 0;
    mouse_state.prev_button_state = 0;
    mouse_state.byte_num = 0;
    mouse_state.initialized = 1;
    
    serial_write_string("Mouse driver initialized at ");
    serial_write_int(initial_x);
    serial_write_string(", ");
    serial_write_int(initial_y);
    serial_write_string("\n");
}

/* Set event callback */
void mouse_set_callback(MouseEventCallback callback) {
    mouse_state.callback = callback;
}

/* Get current mouse position */
int mouse_get_x(void) {
    return mouse_state.x;
}

int mouse_get_y(void) {
    return mouse_state.y;
}

/* Get current button state */
int mouse_get_button_state(void) {
    return mouse_state.button_state;
}

/* Check if mouse is initialized */
int mouse_is_initialized(void) {
    return mouse_state.initialized;
}

/* Fire a mouse event */
static void fire_mouse_event(EventType type, int x, int y, int button) {
    InputEvent event;
    
    if (!mouse_state.callback) return;
    
    event.type = type;
    event.data.mouse.x = x;
    event.data.mouse.y = y;
    event.data.mouse.button = button;
    
    mouse_state.callback(&event);
}

/* Poll for mouse input and generate events */
void mouse_poll(void) {
    unsigned char data;
    signed char dx, dy;
    int old_x, old_y;
    int new_button_state;
    
    if (!mouse_state.initialized) return;
    
    /* Check for serial data on COM1 */
    if (!(inb(0x3FD) & 0x01)) return;
    
    data = inb(0x3F8);
    
    /* Microsoft Serial Mouse protocol parsing */
    if (data & 0x40) {
        /* Start of packet (bit 6 set) */
        mouse_state.bytes[0] = data;
        mouse_state.byte_num = 1;
    } else if (mouse_state.byte_num > 0) {
        mouse_state.bytes[mouse_state.byte_num++] = data;
        
        if (mouse_state.byte_num == 3) {
            /* Complete packet received */
            dx = ((mouse_state.bytes[0] & 0x03) << 6) | (mouse_state.bytes[1] & 0x3F);
            dy = ((mouse_state.bytes[0] & 0x0C) << 4) | (mouse_state.bytes[2] & 0x3F);
            
            /* Sign extend */
            if (dx & 0x80) dx |= 0xFFFFFF00;
            if (dy & 0x80) dy |= 0xFFFFFF00;
            
            /* Save old position */
            old_x = mouse_state.x;
            old_y = mouse_state.y;
            
            /* Update position with 2x speed */
            mouse_state.x += dx * 2;
            mouse_state.y += dy * 2;
            
            /* Clamp to screen bounds (assuming 640x480) */
            if (mouse_state.x < 0) mouse_state.x = 0;
            if (mouse_state.x >= 640) mouse_state.x = 639;
            if (mouse_state.y < 0) mouse_state.y = 0;
            if (mouse_state.y >= 480) mouse_state.y = 479;
            
            /* Generate MOUSE_MOVE event if position changed */
            if (mouse_state.x != old_x || mouse_state.y != old_y) {
                fire_mouse_event(EVENT_MOUSE_MOVE, mouse_state.x, mouse_state.y, 0);
            }
            
            /* Check button states */
            /* Left button is bit 5, right button is bit 4 */
            new_button_state = 0;
            if (mouse_state.bytes[0] & 0x20) new_button_state |= MOUSE_BUTTON_LEFT;
            if (mouse_state.bytes[0] & 0x10) new_button_state |= MOUSE_BUTTON_RIGHT;
            
            /* Generate button events on state change */
            if ((new_button_state & MOUSE_BUTTON_LEFT) && 
                !(mouse_state.button_state & MOUSE_BUTTON_LEFT)) {
                /* Left button pressed */
                fire_mouse_event(EVENT_MOUSE_DOWN, mouse_state.x, mouse_state.y, 1);
            } else if (!(new_button_state & MOUSE_BUTTON_LEFT) && 
                       (mouse_state.button_state & MOUSE_BUTTON_LEFT)) {
                /* Left button released */
                fire_mouse_event(EVENT_MOUSE_UP, mouse_state.x, mouse_state.y, 1);
            }
            
            if ((new_button_state & MOUSE_BUTTON_RIGHT) && 
                !(mouse_state.button_state & MOUSE_BUTTON_RIGHT)) {
                /* Right button pressed */
                fire_mouse_event(EVENT_MOUSE_DOWN, mouse_state.x, mouse_state.y, 2);
            } else if (!(new_button_state & MOUSE_BUTTON_RIGHT) && 
                       (mouse_state.button_state & MOUSE_BUTTON_RIGHT)) {
                /* Right button released */
                fire_mouse_event(EVENT_MOUSE_UP, mouse_state.x, mouse_state.y, 2);
            }
            
            /* Update button state */
            mouse_state.button_state = new_button_state;
            
            /* Reset packet buffer */
            mouse_state.byte_num = 0;
        }
    }
}

/* Set mouse position (for initialization or warping) */
void mouse_set_position(int x, int y) {
    if (x >= 0 && x < 640) mouse_state.x = x;
    if (y >= 0 && y < 480) mouse_state.y = y;
}