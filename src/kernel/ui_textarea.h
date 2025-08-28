/* UI TextArea Component */

#ifndef UI_TEXTAREA_H
#define UI_TEXTAREA_H

#include "view.h"
#include "ui_theme.h"
#include "text_edit_base.h"

#define TEXTAREA_MAX_LINES 256
#define TEXTAREA_MAX_LINE_LENGTH 256
#define TEXTAREA_MAX_TOTAL_CHARS (TEXTAREA_MAX_LINES * TEXTAREA_MAX_LINE_LENGTH)

/* TextArea line structure */
typedef struct {
    char text[TEXTAREA_MAX_LINE_LENGTH];
    int length;
} TextAreaLine;

/* TextArea structure */
typedef struct {
    View base;
    TextEditBase edit_base;  /* Shared text editing functionality */
    
    /* Event bus reference for subscription management */
    struct EventBus *event_bus;   /* Pointer to event bus for subscribing */
    
    /* Pixel dimensions (position is handled by View) */
    int pixel_width, pixel_height;
    
    /* Text data */
    TextAreaLine lines[TEXTAREA_MAX_LINES];
    int line_count;
    int total_chars;
    
    /* Cursor position */
    int cursor_line;
    int cursor_col;
    
    /* View state */
    int scroll_top;      /* First visible line */
    int scroll_left;     /* Horizontal scroll offset */
    int visible_lines;   /* Number of lines that fit in view */
    int visible_cols;    /* Number of columns that fit in view */
    
    /* Additional visual properties not in base */
    /* (Most are now in edit_base) */  /* Reset cursor blink on typing */
    
    /* Future expansion for vim modes, commands, etc */
    void *editor_state;  /* Placeholder for future editor features */
    
} TextArea;

/* Create a new textarea */
TextArea* textarea_create(int x, int y, int width, int height);

/* Set textarea text (replaces all content) */
void textarea_set_text(TextArea *textarea, const char *text);

/* Get current text (concatenates all lines) */
void textarea_get_text(TextArea *textarea, char *buffer, int buffer_size);

/* Insert character at cursor */
void textarea_insert_char(TextArea *textarea, char c);

/* Delete character at cursor */
void textarea_delete_char(TextArea *textarea);

/* Backspace (delete before cursor) */
void textarea_backspace(TextArea *textarea);

/* Cursor movement */
void textarea_move_cursor_up(TextArea *textarea);
void textarea_move_cursor_down(TextArea *textarea);
void textarea_move_cursor_left(TextArea *textarea);
void textarea_move_cursor_right(TextArea *textarea);
void textarea_move_cursor_home(TextArea *textarea);
void textarea_move_cursor_end(TextArea *textarea);

/* Set visual properties */
void textarea_set_colors(TextArea *textarea, unsigned char bg, unsigned char text,
                        unsigned char cursor, unsigned char border, unsigned char focus_border);
void textarea_set_font(TextArea *textarea, FontSize font);

/* Focus management */
void textarea_set_focus(TextArea *textarea, int focus);

/* Internal handlers */
void textarea_handle_key(TextArea *textarea, unsigned char key);

#endif /* UI_TEXTAREA_H */