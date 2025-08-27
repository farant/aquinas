/* Label Component Implementation */

#include "ui_label.h"
#include "graphics_context.h"
#include "grid.h"
#include "dispi.h"
#include "memory.h"

/* Draw label */
void label_draw(View *self, GraphicsContext *gc) {
    Label *label = (Label*)self;
    RegionRect abs_bounds;
    int x, y, w, h;
    int text_x, text_y;
    int char_width, char_height;
    int text_len = 0;
    const char *p;
    
    /* Get absolute bounds in pixels */
    view_get_absolute_bounds(self, &abs_bounds);
    grid_region_to_pixel(abs_bounds.x, abs_bounds.y, &x, &y);
    w = abs_bounds.width * REGION_WIDTH;
    h = abs_bounds.height * REGION_HEIGHT;
    
    /* Fill background if not transparent */
    if (label->bg_color != COLOR_TRANSPARENT) {
        gc_fill_rect(gc, x, y, w, h, label->bg_color);
    }
    
    /* Calculate text dimensions */
    char_width = (label->font == FONT_9X16) ? 9 : 6;
    char_height = (label->font == FONT_9X16) ? 16 : 8;
    
    /* Count text length */
    p = label->text;
    while (*p) {
        text_len++;
        p++;
    }
    
    /* Calculate text position based on alignment */
    text_y = y + (h - char_height) / 2;  /* Vertically centered */
    
    switch (label->align) {
        case ALIGN_CENTER:
            text_x = x + (w - text_len * char_width) / 2;
            break;
            
        case ALIGN_RIGHT:
            text_x = x + w - text_len * char_width - PADDING_SMALL;
            break;
            
        case ALIGN_LEFT:
        default:
            text_x = x + PADDING_SMALL;
            break;
    }
    
    /* Ensure text stays within bounds */
    if (text_x < x) text_x = x;
    if (text_x + text_len * char_width > x + w) {
        /* Text too long, truncate display */
        int max_chars = (x + w - text_x) / char_width;
        if (max_chars > 0) {
            /* Draw as much as fits */
            char temp[256];
            int i;
            for (i = 0; i < max_chars && i < 255 && label->text[i]; i++) {
                temp[i] = label->text[i];
            }
            temp[i] = '\0';
            
            /* Draw text with appropriate font */
            if (label->font == FONT_9X16) {
                dispi_draw_string_bios(text_x, text_y, temp, label->fg_color, 
                                      label->bg_color != COLOR_TRANSPARENT ? label->bg_color : THEME_BG);
            } else {
                dispi_draw_string(text_x, text_y, temp, label->fg_color,
                                 label->bg_color != COLOR_TRANSPARENT ? label->bg_color : THEME_BG);
            }
        }
    } else {
        /* Draw full text */
        if (label->font == FONT_9X16) {
            dispi_draw_string_bios(text_x, text_y, label->text, label->fg_color,
                                  label->bg_color != COLOR_TRANSPARENT ? label->bg_color : THEME_BG);
        } else {
            dispi_draw_string(text_x, text_y, label->text, label->fg_color,
                             label->bg_color != COLOR_TRANSPARENT ? label->bg_color : THEME_BG);
        }
    }
}

/* Create a label */
Label* label_create(int x, int y, int width, const char *text, FontSize font) {
    Label *label;
    int height;
    int region_w, region_h;
    
    label = (Label*)malloc(sizeof(Label));
    if (!label) return NULL;
    
    /* Calculate height based on font */
    height = (font == FONT_9X16) ? 16 + PADDING_SMALL * 2 : 8 + PADDING_SMALL * 2;
    
    /* Convert to regions */
    region_w = (width + REGION_WIDTH - 1) / REGION_WIDTH;
    region_h = (height + REGION_HEIGHT - 1) / REGION_HEIGHT;
    
    /* Initialize base view */
    label->base.bounds.x = x;
    label->base.bounds.y = y;
    label->base.bounds.width = region_w;
    label->base.bounds.height = region_h;
    label->base.parent = NULL;
    label->base.children = NULL;
    label->base.next_sibling = NULL;
    label->base.visible = 1;
    label->base.needs_redraw = 1;
    label->base.z_order = 0;
    label->base.user_data = NULL;
    label->base.draw = label_draw;
    label->base.update = NULL;
    label->base.handle_event = NULL;  /* Labels don't handle events */
    label->base.destroy = NULL;
    label->base.type_name = "Label";
    
    /* Initialize label specific */
    label->text = text;
    label->font = font;
    label->align = ALIGN_LEFT;
    label->fg_color = THEME_FG;
    label->bg_color = COLOR_TRANSPARENT;
    label->wrap = 0;
    
    return label;
}

/* Destroy a label */
void label_destroy(Label *label) {
    if (label) {
        free(label);
    }
}

/* Set label text */
void label_set_text(Label *label, const char *text) {
    if (label && label->text != text) {
        label->text = text;
        view_invalidate((View*)label);
    }
}

/* Set text alignment */
void label_set_align(Label *label, TextAlign align) {
    if (label && label->align != align) {
        label->align = align;
        view_invalidate((View*)label);
    }
}

/* Set colors */
void label_set_colors(Label *label, unsigned char fg, unsigned char bg) {
    if (!label) return;
    
    if (label->fg_color != fg || label->bg_color != bg) {
        label->fg_color = fg;
        label->bg_color = bg;
        view_invalidate((View*)label);
    }
}