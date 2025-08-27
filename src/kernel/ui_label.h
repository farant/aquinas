/* Label Component
 * 
 * A simple text label with alignment support.
 */

#ifndef UI_LABEL_H
#define UI_LABEL_H

#include "view.h"
#include "ui_theme.h"

/* Text alignment */
typedef enum {
    ALIGN_LEFT,
    ALIGN_CENTER,
    ALIGN_RIGHT
} TextAlign;

/* Label structure */
typedef struct Label {
    View base;              /* Inherit from View */
    const char *text;       /* Label text */
    FontSize font;          /* Which font to use */
    TextAlign align;        /* Text alignment */
    unsigned char fg_color; /* Text color */
    unsigned char bg_color; /* Background color (COLOR_TRANSPARENT for none) */
    int wrap;              /* Word wrap enabled */
} Label;

/* Special color for transparent background */
#ifndef COLOR_TRANSPARENT
#define COLOR_TRANSPARENT 255
#endif

/* Label API */
Label* label_create(int x, int y, int width, const char *text, FontSize font);
void label_destroy(Label *label);
void label_set_text(Label *label, const char *text);
void label_set_align(Label *label, TextAlign align);
void label_set_colors(Label *label, unsigned char fg, unsigned char bg);

/* Internal handlers */
void label_draw(View *self, GraphicsContext *gc);

#endif /* UI_LABEL_H */