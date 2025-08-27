/* Text rendering for graphics mode display drivers */

#ifndef TEXT_RENDERER_H
#define TEXT_RENDERER_H

/* Text rendering configuration */
#define TEXT_COLS 106  /* 640 / 6 = 106 chars */
#define TEXT_ROWS 60   /* 480 / 8 = 60 rows */

/* Text renderer structure */
typedef struct {
    int cursor_x;           /* Cursor column position (0-105) */
    int cursor_y;           /* Cursor row position (0-59) */
    unsigned char fg_color; /* Foreground color index */
    unsigned char bg_color; /* Background color index */
    int cursor_visible;     /* Is cursor visible? */
    int cursor_blink_state; /* Current blink state */
    unsigned int last_blink_time; /* Last time cursor toggled */
} TextRenderer;

/* Initialize text renderer */
void text_renderer_init(void);

/* Clear the screen */
void text_renderer_clear(void);

/* Draw a character at the current cursor position */
void text_renderer_putchar(char c);

/* Draw a string at the current cursor position */
void text_renderer_puts(const char *str);

/* Draw a character at specific position */
void text_renderer_draw_char(int col, int row, char c, unsigned char fg, unsigned char bg);

/* Draw a string at specific position */
void text_renderer_draw_string(int col, int row, const char *str, unsigned char fg, unsigned char bg);

/* Set cursor position */
void text_renderer_set_cursor(int col, int row);

/* Get cursor position */
void text_renderer_get_cursor(int *col, int *row);

/* Show/hide cursor */
void text_renderer_show_cursor(int visible);

/* Update cursor blink (call periodically) */
void text_renderer_update_cursor(void);

/* Draw the cursor at current position */
void text_renderer_draw_cursor(void);

/* Set text colors */
void text_renderer_set_colors(unsigned char fg, unsigned char bg);

/* Scroll the screen up one line */
void text_renderer_scroll(void);

/* Get the global text renderer instance */
TextRenderer* text_renderer_get(void);

#endif /* TEXT_RENDERER_H */