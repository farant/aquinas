#ifndef MOUSE_H
#define MOUSE_H

#include "view.h"  /* For InputEvent type */

/* Mouse button masks */
#define MOUSE_BUTTON_LEFT   0x01
#define MOUSE_BUTTON_RIGHT  0x02
#define MOUSE_BUTTON_MIDDLE 0x04

/* Mouse event callback type */
typedef void (*MouseEventCallback)(InputEvent *event);

/* Mouse state structure */
typedef struct {
    int x, y;                       /* Current position in pixels */
    int button_state;               /* Current button states */
    int prev_button_state;          /* Previous frame's button states */
    unsigned char bytes[3];         /* Protocol buffer */
    int byte_num;                   /* Current byte in packet */
    MouseEventCallback callback;    /* Event callback function */
    int initialized;                /* Is driver initialized */
} MouseState;

/* Mouse API functions */
void mouse_init(int initial_x, int initial_y);
void mouse_poll(void);
void mouse_set_callback(MouseEventCallback callback);
void mouse_set_position(int x, int y);

/* Mouse state queries */
int mouse_get_x(void);
int mouse_get_y(void);
int mouse_get_button_state(void);
int mouse_is_initialized(void);

#endif /* MOUSE_H */