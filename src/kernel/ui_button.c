/* Button Component Implementation */

#include "ui_button.h"
#include "graphics_context.h"
#include "grid.h"
#include "dispi.h"
#include "memory.h"
#include "serial.h"

/* Calculate button width based on label */
static int calculate_button_width(const char *label, FontSize font) {
    int char_width = (font == FONT_9X16) ? 9 : 6;
    int len = 0;
    const char *p = label;
    
    while (*p) {
        len++;
        p++;
    }
    
    /* Add padding: 4 chars worth on each side */
    return (len + 8) * char_width;
}

/* Calculate button height based on font */
static int calculate_button_height(FontSize font) {
    int char_height = (font == FONT_9X16) ? 16 : 8;
    /* Add padding: 1 line above and below */
    return char_height + PADDING_LARGE * 2;
}

/* Draw button */
void button_draw(View *self, GraphicsContext *gc) {
    Button *button = (Button*)self;
    RegionRect abs_bounds;
    int x, y, w, h;
    unsigned char bg_color, fg_color, border_color;
    int text_x, text_y;
    int char_width, char_height;
    int label_len = 0;
    const char *p;
    
    /* Get absolute bounds in pixels */
    view_get_absolute_bounds(self, &abs_bounds);
    grid_region_to_pixel(abs_bounds.x, abs_bounds.y, &x, &y);
    w = abs_bounds.width * REGION_WIDTH;
    h = abs_bounds.height * REGION_HEIGHT;
    
    /* Determine colors based on state and style */
    switch (button->state) {
        case BUTTON_STATE_DISABLED:
            bg_color = THEME_BG;
            fg_color = COLOR_MED_DARK_GRAY;
            border_color = COLOR_MED_DARK_GRAY;
            break;
            
        case BUTTON_STATE_PRESSED:
            /* Offset for pressed effect */
            x += button->pressed_offset;
            y += button->pressed_offset;
            
            if (button->style == BUTTON_STYLE_PRIMARY) {
                bg_color = COLOR_DARK_CYAN;
                fg_color = COLOR_WHITE;
            } else if (button->style == BUTTON_STYLE_DANGER) {
                bg_color = COLOR_DARK_RED;
                fg_color = COLOR_WHITE;
            } else {
                bg_color = THEME_BUTTON_PRESS;
                fg_color = THEME_FG;
            }
            border_color = COLOR_DARK_GRAY;
            break;
            
        case BUTTON_STATE_HOVER:
            if (button->style == BUTTON_STYLE_PRIMARY) {
                bg_color = THEME_ACCENT_CYAN;
                fg_color = COLOR_BLACK;
            } else if (button->style == BUTTON_STYLE_DANGER) {
                bg_color = THEME_ACCENT_RED;
                fg_color = COLOR_WHITE;
            } else {
                bg_color = THEME_BUTTON_HOVER;
                fg_color = THEME_FG;
            }
            border_color = THEME_FOCUS;
            break;
            
        case BUTTON_STATE_NORMAL:
        default:
            if (button->style == BUTTON_STYLE_PRIMARY) {
                bg_color = COLOR_MED_CYAN;
                fg_color = COLOR_BLACK;
            } else if (button->style == BUTTON_STYLE_DANGER) {
                bg_color = COLOR_MED_RED;
                fg_color = COLOR_WHITE;
            } else {
                bg_color = THEME_BUTTON_BG;
                fg_color = THEME_FG;
            }
            border_color = THEME_BORDER;
            break;
    }
    
    /* Fill background */
    gc_fill_rect(gc, x, y, w, h, bg_color);
    
    /* Draw 3D border effect */
    if (button->state != BUTTON_STATE_PRESSED) {
        /* Raised effect - light on top/left, dark on bottom/right */
        gc_draw_line(gc, x, y, x + w - 1, y, COLOR_WHITE);           /* Top */
        gc_draw_line(gc, x, y, x, y + h - 1, COLOR_WHITE);           /* Left */
        gc_draw_line(gc, x + w - 1, y + 1, x + w - 1, y + h - 1, COLOR_DARK_GRAY); /* Right */
        gc_draw_line(gc, x + 1, y + h - 1, x + w - 1, y + h - 1, COLOR_DARK_GRAY); /* Bottom */
    } else {
        /* Sunken effect - dark on top/left, light on bottom/right */
        gc_draw_line(gc, x, y, x + w - 1, y, COLOR_DARK_GRAY);       /* Top */
        gc_draw_line(gc, x, y, x, y + h - 1, COLOR_DARK_GRAY);       /* Left */
        gc_draw_line(gc, x + w - 1, y + 1, x + w - 1, y + h - 1, COLOR_MED_GRAY);  /* Right */
        gc_draw_line(gc, x + 1, y + h - 1, x + w - 1, y + h - 1, COLOR_MED_GRAY);  /* Bottom */
    }
    
    /* Draw focus ring if hover */
    if (button->state == BUTTON_STATE_HOVER) {
        gc_draw_rect(gc, x + 1, y + 1, w - 2, h - 2, border_color);
    }
    
    /* Calculate text position (centered) */
    char_width = (button->font == FONT_9X16) ? 9 : 6;
    char_height = (button->font == FONT_9X16) ? 16 : 8;
    
    /* Count label length */
    p = button->label;
    while (*p) {
        label_len++;
        p++;
    }
    
    text_x = x + (w - label_len * char_width) / 2;
    text_y = y + (h - char_height) / 2;
    
    /* Draw label */
    if (button->font == FONT_9X16) {
        dispi_draw_string_bios(text_x, text_y, button->label, fg_color, bg_color);
    } else {
        dispi_draw_string(text_x, text_y, button->label, fg_color, bg_color);
    }
}

/* Handle button events */
int button_handle_event(View *self, InputEvent *event) {
    Button *button = (Button*)self;
    
    if (button->state == BUTTON_STATE_DISABLED) {
        return 0;  /* Disabled buttons don't handle events */
    }
    
    switch (event->type) {
        case EVENT_MOUSE_MOVE:
            /* Update hover state */
            if (button->state != BUTTON_STATE_PRESSED) {
                button->state = BUTTON_STATE_HOVER;
                view_invalidate(self);
            }
            return 1;
            
        case EVENT_MOUSE_DOWN:
            /* Press button */
            button->state = BUTTON_STATE_PRESSED;
            view_invalidate(self);
            return 1;
            
        case EVENT_MOUSE_UP:
            /* Release button and fire callback if still over button */
            if (button->state == BUTTON_STATE_PRESSED) {
                button->state = BUTTON_STATE_HOVER;
                view_invalidate(self);
                
                /* Fire callback */
                if (button->on_click) {
                    button->on_click(button, button->user_data);
                }
                
                serial_write_string("Button clicked: ");
                serial_write_string(button->label);
                serial_write_string("\n");
            }
            return 1;
            
        default:
            break;
    }
    
    return 0;  /* Event not handled */
}

/* Create a button */
Button* button_create(int x, int y, const char *label, FontSize font) {
    Button *button;
    int width, height;
    int region_w, region_h;
    
    button = (Button*)malloc(sizeof(Button));
    if (!button) return NULL;
    
    /* Calculate dimensions */
    width = calculate_button_width(label, font);
    height = calculate_button_height(font);
    
    /* Convert pixels to regions (approximate) */
    /* This is simplified - in practice we'd want pixel-precise positioning */
    region_w = (width + REGION_WIDTH - 1) / REGION_WIDTH;
    region_h = (height + REGION_HEIGHT - 1) / REGION_HEIGHT;
    
    /* Initialize base view */
    button->base.bounds.x = x;
    button->base.bounds.y = y;
    button->base.bounds.width = region_w;
    button->base.bounds.height = region_h;
    button->base.parent = NULL;
    button->base.children = NULL;
    button->base.next_sibling = NULL;
    button->base.visible = 1;
    button->base.needs_redraw = 1;
    button->base.z_order = 0;
    button->base.user_data = NULL;
    button->base.draw = button_draw;
    button->base.update = NULL;
    button->base.handle_event = button_handle_event;
    button->base.destroy = NULL;
    button->base.type_name = "Button";
    
    /* Initialize button specific */
    button->label = label;
    button->font = font;
    button->state = BUTTON_STATE_NORMAL;
    button->style = BUTTON_STYLE_NORMAL;
    button->on_click = NULL;
    button->user_data = NULL;
    button->min_width = width;
    button->pressed_offset = 1;
    
    return button;
}

/* Destroy a button */
void button_destroy(Button *button) {
    if (button) {
        /* Note: We don't free the label as it's assumed to be static or managed elsewhere */
        free(button);
    }
}

/* Set button style */
void button_set_style(Button *button, ButtonStyle style) {
    if (button && button->style != style) {
        button->style = style;
        view_invalidate((View*)button);
    }
}

/* Enable/disable button */
void button_set_enabled(Button *button, int enabled) {
    ButtonState new_state;
    
    if (!button) return;
    
    new_state = enabled ? BUTTON_STATE_NORMAL : BUTTON_STATE_DISABLED;
    if (button->state != new_state) {
        button->state = new_state;
        view_invalidate((View*)button);
    }
}

/* Set click callback */
void button_set_callback(Button *button, ButtonClickCallback callback, void *user_data) {
    if (button) {
        button->on_click = callback;
        button->user_data = user_data;
    }
}