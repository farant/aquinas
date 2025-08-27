#include "modes.h"

/* Global mode state */
EditorMode editor_mode = MODE_INSERT;  /* Start in insert mode */

/* Forward declaration for draw_nav_bar - will be in display module */
extern void draw_nav_bar(void);

/* Set the current editor mode */
void set_mode(EditorMode mode) {
    editor_mode = mode;
    /* Refresh to show new mode in status */
    draw_nav_bar();
}

/* Get string representation of current mode */
const char* get_mode_string(void) {
    switch (editor_mode) {
        case MODE_NORMAL:
            return "NORMAL";
        case MODE_INSERT:
            return "INSERT";
        case MODE_VISUAL:
            return "VISUAL";
        default:
            return "UNKNOWN";
    }
}