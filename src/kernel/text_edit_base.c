/* Shared Text Editing Base Implementation */

#include "text_edit_base.h"
#include "view.h"
#include "grid.h"
#include "serial.h"

#define NULL ((void*)0)
#define CURSOR_BLINK_RATE 30  /* Frames between blinks */
#define TYPING_TIMEOUT 30      /* Frames before cursor resumes blinking */

/* Initialize text edit base properties */
void text_edit_base_init(TextEditBase *base) {
    if (!base) return;
    
    /* Initialize state */
    base->state = TEXT_STATE_NORMAL;
    base->has_focus = 0;
    
    /* Initialize cursor */
    base->cursor_visible = 1;
    base->cursor_blink_timer = 0;
    base->typing_timer = 0;
    
    /* Default colors */
    base->bg_color = COLOR_WHITE;
    base->text_color = COLOR_BLACK;
    base->cursor_color = COLOR_MED_GOLD;
    base->border_color = COLOR_MED_GRAY;
    base->focus_border_color = COLOR_MED_CYAN;
    base->disabled_bg_color = THEME_BG;
    base->disabled_text_color = COLOR_MED_DARK_GRAY;
    base->font = FONT_6X8;
    
    /* No callbacks by default */
    base->on_focus_change = NULL;
    base->on_text_change = NULL;
}

/* Set focus state */
void text_edit_base_set_focus(TextEditBase *base, View *view, int focus) {
    int old_focus;
    
    if (!base || !view) return;
    
    old_focus = base->has_focus;
    
    /* Update focus state */
    base->has_focus = focus;
    base->state = focus ? TEXT_STATE_FOCUSED : TEXT_STATE_NORMAL;
    
    if (focus) {
        /* Reset cursor when gaining focus */
        base->cursor_visible = 1;
        base->cursor_blink_timer = 0;
        base->typing_timer = 0;
    }
    
    /* Call callback if focus changed */
    if (old_focus != focus) {
        if (base->on_focus_change) {
            base->on_focus_change(view, focus);
        }
        /* Mark for redraw to update border */
        view_invalidate(view);
    }
}

/* Check if focused */
int text_edit_base_is_focused(TextEditBase *base) {
    return base ? base->has_focus : 0;
}

/* Update cursor blink animation */
void text_edit_base_update_cursor(TextEditBase *base) {
    if (!base || !base->has_focus) return;
    
    /* Keep cursor solid while typing */
    if (base->typing_timer > 0) {
        base->typing_timer--;
        base->cursor_visible = 1;
        base->cursor_blink_timer = 0;
    } else {
        /* Blink cursor */
        base->cursor_blink_timer++;
        if (base->cursor_blink_timer >= CURSOR_BLINK_RATE) {
            base->cursor_visible = !base->cursor_visible;
            base->cursor_blink_timer = 0;
        }
    }
}

/* Reset typing timer */
void text_edit_base_reset_typing_timer(TextEditBase *base) {
    if (!base) return;
    base->typing_timer = TYPING_TIMEOUT;
    base->cursor_visible = 1;
    base->cursor_blink_timer = 0;
}

/* Get current colors based on state */
void text_edit_base_get_colors(TextEditBase *base, 
                               unsigned char *bg_out, 
                               unsigned char *text_out,
                               unsigned char *border_out) {
    if (!base) return;
    
    switch (base->state) {
        case TEXT_STATE_DISABLED:
            if (bg_out) *bg_out = base->disabled_bg_color;
            if (text_out) *text_out = base->disabled_text_color;
            if (border_out) *border_out = COLOR_MED_DARK_GRAY;
            break;
            
        case TEXT_STATE_FOCUSED:
            if (bg_out) *bg_out = base->bg_color;
            if (text_out) *text_out = base->text_color;
            if (border_out) *border_out = base->focus_border_color;
            break;
            
        case TEXT_STATE_NORMAL:
        default:
            if (bg_out) *bg_out = base->bg_color;
            if (text_out) *text_out = base->text_color;
            if (border_out) *border_out = base->border_color;
            break;
    }
}

/* Check if point is inside view bounds */
int text_edit_base_hit_test(View *view, int pixel_x, int pixel_y) {
    RegionRect abs_bounds;
    int view_x, view_y, view_w, view_h;
    
    if (!view) return 0;
    
    /* Get view's absolute position and size */
    view_get_absolute_bounds(view, &abs_bounds);
    grid_region_to_pixel(abs_bounds.x, abs_bounds.y, &view_x, &view_y);
    view_w = abs_bounds.width * REGION_WIDTH;
    view_h = abs_bounds.height * REGION_HEIGHT;
    
    /* Check if point is inside */
    return (pixel_x >= view_x && pixel_x < view_x + view_w &&
            pixel_y >= view_y && pixel_y < view_y + view_h);
}

/* Handle common mouse down event */
int text_edit_base_handle_mouse_down(TextEditBase *base, View *view, InputEvent *event) {
    if (!base || !view || !event) return 0;
    
    /* Check if click is inside our bounds */
    if (text_edit_base_hit_test(view, event->data.mouse.x, event->data.mouse.y)) {
        /* Set focus if not already focused */
        if (!base->has_focus) {
            serial_write_string("text_edit_base: Setting focus\n");
            text_edit_base_set_focus(base, view, 1);
        }
        return 1;  /* Event handled */
    } else {
        /* Click outside - lose focus */
        if (base->has_focus) {
            serial_write_string("text_edit_base: Losing focus\n");
            text_edit_base_set_focus(base, view, 0);
        }
        return 0;  /* Event not handled */
    }
}