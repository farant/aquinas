#ifndef MOUSE_H
#define MOUSE_H

/* Mouse state */
extern int mouse_x;          /* Mouse X position (0-79) */
extern int mouse_y;          /* Mouse Y position (0-24) */
extern int mouse_visible;    /* Is mouse cursor visible */

/* Mouse functions */
void init_mouse(void);
void poll_mouse(void);

#endif /* MOUSE_H */