/* Shared Text Editing Base Component
 * 
 * Common functionality for text editing components like TextInput and TextArea.
 * Provides focus management, cursor handling, and basic text operations.
 */

#ifndef TEXT_EDIT_BASE_H
#define TEXT_EDIT_BASE_H

#include "view.h"
#include "ui_theme.h"

/* Text editor states */
typedef enum {
    TEXT_STATE_NORMAL = 0,
    TEXT_STATE_FOCUSED,
    TEXT_STATE_DISABLED
} TextEditState;

/* Common text editing properties */
typedef struct {
    /* Focus and state */
    TextEditState state;
    int has_focus;
    
    /* Cursor */
    int cursor_visible;
    int cursor_blink_timer;
    int typing_timer;  /* Keeps cursor solid while typing */
    
    /* Visual properties */
    unsigned char bg_color;
    unsigned char text_color;
    unsigned char cursor_color;
    unsigned char border_color;
    unsigned char focus_border_color;
    unsigned char disabled_bg_color;
    unsigned char disabled_text_color;
    FontSize font;
    
    /* Callbacks */
    void (*on_focus_change)(void *component, int focused);
    void (*on_text_change)(void *component);
} TextEditBase;

/* Initialize text edit base properties */
void text_edit_base_init(TextEditBase *base);

/* Focus management */
void text_edit_base_set_focus(TextEditBase *base, View *view, int focus);
int text_edit_base_is_focused(TextEditBase *base);

/* Update cursor blink */
void text_edit_base_update_cursor(TextEditBase *base);

/* Reset typing timer (call when key pressed) */
void text_edit_base_reset_typing_timer(TextEditBase *base);

/* Get current colors based on state */
void text_edit_base_get_colors(TextEditBase *base, 
                               unsigned char *bg_out, 
                               unsigned char *text_out,
                               unsigned char *border_out);

/* Handle common mouse events (returns 1 if handled) */
int text_edit_base_handle_mouse_down(TextEditBase *base, View *view, InputEvent *event);

/* Check if point is inside view bounds (pixel coordinates) */
int text_edit_base_hit_test(View *view, int pixel_x, int pixel_y);

#endif /* TEXT_EDIT_BASE_H */