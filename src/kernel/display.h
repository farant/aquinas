#ifndef DISPLAY_H
#define DISPLAY_H

#include "vga.h"
#include "page.h"

/* Mouse state */
extern int mouse_x;
extern int mouse_y;
extern int mouse_visible;

/* Graphics mode flag */
extern int graphics_mode_active;

/* Display functions */
void draw_nav_bar(void);
void update_cursor(void);
void refresh_screen(void);
void clear_screen(void);

#endif /* DISPLAY_H */