/* Panel Component
 * 
 * A container component with border and background.
 */

#ifndef UI_PANEL_H
#define UI_PANEL_H

#include "view.h"
#include "ui_theme.h"

/* Panel structure */
typedef struct Panel {
    View base;                  /* Inherit from View */
    const char *title;          /* Optional title */
    FontSize title_font;        /* Font for title */
    BorderStyle border_style;   /* Border style */
    unsigned char bg_color;     /* Background color */
    unsigned char border_color; /* Border color */
    int padding;               /* Internal padding in pixels */
} Panel;

/* Panel API */
Panel* panel_create(int x, int y, int width, int height);
void panel_destroy(Panel *panel);
void panel_set_title(Panel *panel, const char *title, FontSize font);
void panel_set_border(Panel *panel, BorderStyle style, unsigned char color);
void panel_set_background(Panel *panel, unsigned char color);
void panel_add_child(Panel *panel, View *child);

/* Internal handlers */
void panel_draw(View *self, GraphicsContext *gc);

#endif /* UI_PANEL_H */