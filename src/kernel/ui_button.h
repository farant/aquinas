/* Button Component
 * 
 * A clickable button with label, supporting both font sizes.
 */

#ifndef UI_BUTTON_H
#define UI_BUTTON_H

#include "view.h"
#include "ui_theme.h"

/* Button states */
typedef enum {
    BUTTON_STATE_NORMAL,
    BUTTON_STATE_HOVER,
    BUTTON_STATE_PRESSED,
    BUTTON_STATE_DISABLED
} ButtonState;

/* Button styles */
typedef enum {
    BUTTON_STYLE_NORMAL,   /* Standard gray button */
    BUTTON_STYLE_PRIMARY,  /* Cyan accent button */
    BUTTON_STYLE_DANGER    /* Red accent button */
} ButtonStyle;

/* Forward declaration */
typedef struct Button Button;

/* Button click callback */
typedef void (*ButtonClickCallback)(Button *button, void *user_data);

/* Button structure */
struct Button {
    View base;                    /* Inherit from View */
    const char *label;           /* Button text */
    FontSize font;               /* Which font to use */
    ButtonState state;           /* Current button state */
    ButtonStyle style;           /* Visual style */
    ButtonClickCallback on_click; /* Click handler */
    void *user_data;             /* User data for callback */
    int min_width;               /* Minimum width in pixels */
    int pressed_offset;          /* Offset when pressed for 3D effect */
    /* Pixel-precise bounds for accurate hit testing */
    int pixel_x, pixel_y;        /* Actual position in pixels */
    int pixel_width, pixel_height; /* Actual size in pixels */
    
    /* Event bus reference for subscription management */
    struct EventBus *event_bus;   /* Pointer to event bus for subscribing */
};

/* Button API */
Button* button_create(int x, int y, const char *label, FontSize font);
void button_destroy(Button *button);
void button_set_style(Button *button, ButtonStyle style);
void button_set_enabled(Button *button, int enabled);
void button_set_callback(Button *button, ButtonClickCallback callback, void *user_data);

/* Internal handlers (implemented in ui_button.c) */
void button_draw(View *self, GraphicsContext *gc);
int button_handle_event(View *self, InputEvent *event);

#endif /* UI_BUTTON_H */