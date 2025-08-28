/* Text Input Component
 * 
 * Single-line text input field with cursor and editing support.
 */

#ifndef UI_TEXTINPUT_H
#define UI_TEXTINPUT_H

#include "view.h"
#include "ui_theme.h"
#include "text_edit_base.h"

/* Text input states - now using shared base states */
typedef enum {
    TEXTINPUT_STATE_NORMAL = TEXT_STATE_NORMAL,
    TEXTINPUT_STATE_FOCUSED = TEXT_STATE_FOCUSED,
    TEXTINPUT_STATE_DISABLED = TEXT_STATE_DISABLED
} TextInputState;

/* Text input structure */
typedef struct TextInput {
    View base;                    /* Inherit from View */
    TextEditBase edit_base;       /* Shared text editing functionality */
    
    /* Text buffer */
    char *buffer;                 /* Text buffer */
    int buffer_size;              /* Maximum buffer size */
    int text_length;              /* Current text length */
    
    /* Cursor and selection */
    int cursor_pos;               /* Cursor position (0 to text_length) */
    int selection_start;          /* Selection start (-1 if no selection) */
    int selection_end;            /* Selection end */
    
    /* Display */
    int scroll_offset;            /* Horizontal scroll offset for long text */
    const char *placeholder;      /* Placeholder text when empty */
    
    /* Callbacks */
    void (*on_change)(struct TextInput *input, void *user_data);
    void (*on_submit)(struct TextInput *input, void *user_data);
    void *user_data;
    
    /* Visual properties */
    int pixel_x, pixel_y;         /* Pixel position */
    int pixel_width, pixel_height; /* Pixel dimensions */
} TextInput;

/* TextInput API */
TextInput* textinput_create(int x, int y, int width, const char *placeholder, FontSize font);
void textinput_destroy(TextInput *input);

/* Content management */
void textinput_set_text(TextInput *input, const char *text);
const char* textinput_get_text(TextInput *input);
void textinput_clear(TextInput *input);
void textinput_set_max_length(TextInput *input, int max_length);

/* State management */
void textinput_set_focused(TextInput *input, int focused);
void textinput_set_enabled(TextInput *input, int enabled);

/* Callbacks */
void textinput_set_on_change(TextInput *input, 
                             void (*on_change)(TextInput*, void*), 
                             void *user_data);
void textinput_set_on_submit(TextInput *input,
                             void (*on_submit)(TextInput*, void*),
                             void *user_data);

/* Internal handlers */
void textinput_draw(View *self, GraphicsContext *gc);
void textinput_update(View *self, int delta_ms);
int textinput_handle_event(View *self, InputEvent *event);

#endif /* UI_TEXTINPUT_H */