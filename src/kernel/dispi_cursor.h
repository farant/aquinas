/* DISPI Mouse Cursor Support */

#ifndef DISPI_CURSOR_H
#define DISPI_CURSOR_H

/* Cursor dimensions */
#define CURSOR_WIDTH 12
#define CURSOR_HEIGHT 20
#define CURSOR_HOTSPOT_X 0
#define CURSOR_HOTSPOT_Y 0

/* Maximum background buffer size (including 1-pixel border) */
#define CURSOR_BG_SIZE ((CURSOR_WIDTH + 2) * (CURSOR_HEIGHT + 2))

/* Initialize the cursor system */
void dispi_cursor_init(void);

/* Show the cursor at current position */
void dispi_cursor_show(void);

/* Hide the cursor */
void dispi_cursor_hide(void);

/* Update cursor position */
void dispi_cursor_move(int x, int y);

/* Get current cursor position */
void dispi_cursor_get_pos(int *x, int *y);

/* Check if cursor is visible */
int dispi_cursor_is_visible(void);

#endif /* DISPI_CURSOR_H */