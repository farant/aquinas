/* UI TextArea Component Implementation */

#include "ui_textarea.h"
#include "text_edit_base.h"
#include "view_interface.h"
#include "graphics_context.h"
#include "grid.h"
#include "dispi.h"
#include "memory.h"
#include "serial.h"
#include "event_bus.h"

/* Constants - use the PADDING values from ui_theme.h */
#define TEXTAREA_PADDING 5
#define CURSOR_BLINK_RATE 30  /* Frames between blinks */
#define TYPING_TIMEOUT 30      /* Frames before cursor resumes blinking */
#define LINE_HEIGHT_6X8 10     /* 8 + 2 pixel spacing */
#define LINE_HEIGHT_9X16 18    /* 16 + 2 pixel spacing */

/* Forward declarations */
static void textarea_draw(View *view, GraphicsContext *gc);
static int textarea_handle_event(View *view, InputEvent *event);
static void textarea_destroy(View *view);
static void ensure_cursor_visible(TextArea *textarea);
static int get_line_at_y(TextArea *textarea, int y);
static int get_col_at_x(TextArea *textarea, int line_idx, int x);
static int textarea_keyboard_handler(View *view, InputEvent *event, void *context);
static int textarea_mouse_handler(View *view, InputEvent *event, void *context);

/* Interface callback declarations */
static void textarea_interface_init(View *view, ViewContext *context);
static void textarea_interface_destroy(View *view);
static void textarea_interface_on_focus_gained(View *view);
static void textarea_interface_on_focus_lost(View *view);
static int textarea_interface_can_focus(View *view);
static RegionRect textarea_interface_get_preferred_size(View *view);

/* TextArea ViewInterface definition */
static ViewInterface textarea_interface = {
    /* Lifecycle methods */
    textarea_interface_init,
    textarea_interface_destroy,
    
    /* Parent-child callbacks */
    NULL,  /* Use defaults */
    NULL,
    NULL,
    NULL,
    
    /* State changes */
    textarea_interface_on_focus_gained,
    textarea_interface_on_focus_lost,
    NULL,  /* Use default for visibility */
    NULL,  /* Use default for enabled */
    
    /* Capabilities */
    textarea_interface_can_focus,
    textarea_interface_get_preferred_size
};

/* ViewInterface callback implementations */

static void textarea_interface_init(View *view, ViewContext *context) {
    TextArea *textarea = (TextArea*)view;
    
    serial_write_string("TextArea: Interface init called\n");
    
    /* Store event bus reference if available */
    if (context && context->event_bus) {
        textarea->event_bus = context->event_bus;
        serial_write_string("TextArea: Event bus stored for future subscription\n");
    } else {
        textarea->event_bus = NULL;
    }
    
    /* Initialize shared text editing base */
    text_edit_base_init(&textarea->edit_base);
}

static void textarea_interface_destroy(View *view) {
    TextArea *textarea = (TextArea*)view;
    
    serial_write_string("TextArea: Interface destroy called\n");
    
    /* Unsubscribe from event bus if we have focus */
    if (textarea->event_bus && textarea->edit_base.has_focus) {
        event_bus_unsubscribe(textarea->event_bus, view, EVENT_KEY_DOWN);
        event_bus_unsubscribe(textarea->event_bus, view, EVENT_MOUSE_DOWN);
        serial_write_string("TextArea: Unsubscribed from event bus on destroy\n");
    }
}

static void textarea_interface_on_focus_gained(View *view) {
    TextArea *textarea = (TextArea*)view;
    
    serial_write_string("TextArea: Got focus via interface!\n");
    
    /* Subscribe to event bus for keyboard and mouse events */
    if (textarea->event_bus) {
        /* Subscribe keyboard at NORMAL priority to allow system shortcuts first */
        event_bus_subscribe(textarea->event_bus, view, EVENT_KEY_DOWN, 
                          EVENT_PRIORITY_NORMAL, textarea_keyboard_handler, textarea);
        
        /* Subscribe mouse at NORMAL priority */
        event_bus_subscribe(textarea->event_bus, view, EVENT_MOUSE_DOWN,
                          EVENT_PRIORITY_NORMAL, textarea_mouse_handler, textarea);
        
        serial_write_string("TextArea: Subscribed to event bus for keyboard and mouse\n");
    }
    
    /* Update focus state using shared base */
    text_edit_base_set_focus(&textarea->edit_base, view, 1);
    
    /* Mark for redraw */
    view->needs_redraw = 1;
}

static void textarea_interface_on_focus_lost(View *view) {
    TextArea *textarea = (TextArea*)view;
    
    serial_write_string("TextArea: Lost focus via interface!\n");
    
    /* Unsubscribe from event bus */
    if (textarea->event_bus) {
        event_bus_unsubscribe(textarea->event_bus, view, EVENT_KEY_DOWN);
        event_bus_unsubscribe(textarea->event_bus, view, EVENT_MOUSE_DOWN);
        serial_write_string("TextArea: Unsubscribed from event bus\n");
    }
    
    /* Update focus state using shared base */
    text_edit_base_set_focus(&textarea->edit_base, view, 0);
    
    /* Mark for redraw */
    view->needs_redraw = 1;
}

static int textarea_interface_can_focus(View *view) {
    TextArea *textarea = (TextArea*)view;
    
    /* Can focus if not disabled */
    return textarea->edit_base.state != TEXT_STATE_DISABLED;
}

static RegionRect textarea_interface_get_preferred_size(View *view) {
    /* Return current bounds as preferred size */
    return view->bounds;
}

/* Create a new textarea */
TextArea* textarea_create(int x, int y, int width, int height) {
    TextArea *textarea = (TextArea*)malloc(sizeof(TextArea));
    View *view = (View*)textarea;
    int i;
    
    if (!textarea) {
        serial_write_string("Failed to allocate TextArea\n");
        return NULL;
    }
    
    /* Initialize base view - use grid coordinates like other UI components */
    view->bounds.x = x;      /* Grid x position */
    view->bounds.y = y;      /* Grid y position */
    view->bounds.width = width;   /* Grid width */
    view->bounds.height = height;  /* Grid height */
    view->parent = NULL;
    view->children = NULL;
    view->next_sibling = NULL;
    view->visible = 1;
    view->needs_redraw = 1;
    view->z_order = 0;
    view->user_data = NULL;
    view->draw = textarea_draw;
    view->update = NULL;
    view->handle_event = textarea_handle_event;
    view->destroy = textarea_destroy;
    view->type_name = "TextArea";
    view->interface = &textarea_interface;  /* Set ViewInterface */
    
    /* Calculate pixel dimensions (not position - View handles that) */
    /* These are the actual pixel dimensions of the textarea content area */
    textarea->pixel_width = width * REGION_WIDTH;
    textarea->pixel_height = height * REGION_HEIGHT;
    
    /* Initialize text data */
    textarea->line_count = 1;  /* Start with one empty line */
    textarea->total_chars = 0;
    for (i = 0; i < TEXTAREA_MAX_LINES; i++) {
        textarea->lines[i].text[0] = '\0';
        textarea->lines[i].length = 0;
    }
    
    /* Initialize cursor */
    textarea->cursor_line = 0;
    textarea->cursor_col = 0;
    
    /* Initialize scroll */
    textarea->scroll_top = 0;
    textarea->scroll_left = 0;
    
    /* Initialize shared text edit base */
    text_edit_base_init(&textarea->edit_base);
    textarea->edit_base.font = FONT_6X8;
    textarea->edit_base.bg_color = COLOR_DARK_GRAY;
    textarea->edit_base.text_color = COLOR_WHITE;
    textarea->edit_base.focus_border_color = COLOR_BRIGHT_GOLD;
    
    /* Initialize the view through its interface */
    if (view->interface) {
        ViewContext context = {NULL, NULL, NULL, NULL};
        view_interface_init(view, view->interface, &context);
    }
    
    /* Calculate visible area based on pixel dimensions */
    textarea->visible_lines = (textarea->pixel_height - TEXTAREA_PADDING * 2 - 2) / LINE_HEIGHT_6X8;
    textarea->visible_cols = (textarea->pixel_width - TEXTAREA_PADDING * 2) / 6;
    
    /* State */
    textarea->editor_state = NULL;
    
    return textarea;
}

/* Draw the textarea */
static void textarea_draw(View *view, GraphicsContext *gc) {
    TextArea *textarea = (TextArea*)view;
    RegionRect abs_bounds;
    int x, y;
    int line_height = (textarea->edit_base.font == FONT_9X16) ? LINE_HEIGHT_9X16 : LINE_HEIGHT_6X8;
    int char_width = (textarea->edit_base.font == FONT_9X16) ? 9 : 6;
    int char_height = (textarea->edit_base.font == FONT_9X16) ? 16 : 8;
    int i, j, line_y, char_x;
    unsigned char bg_color, text_color, border_color;
    TextAreaLine *line;
    
    /* Get absolute position from parent hierarchy */
    view_get_absolute_bounds(view, &abs_bounds);
    grid_region_to_pixel(abs_bounds.x, abs_bounds.y, &x, &y);
    
    /* Use the view's actual dimensions in pixels */
    textarea->pixel_width = abs_bounds.width * REGION_WIDTH;
    textarea->pixel_height = abs_bounds.height * REGION_HEIGHT;
    
    /* Get colors from shared base */
    text_edit_base_get_colors(&textarea->edit_base, &bg_color, &text_color, &border_color);
    
    /* Clear background */
    gc_fill_rect(gc, x, y, textarea->pixel_width, textarea->pixel_height, bg_color);
    
    /* Draw border */
    gc_draw_rect(gc, x, y, textarea->pixel_width, textarea->pixel_height, border_color);
    
    /* Draw visible lines */
    for (i = 0; i < textarea->visible_lines && (i + textarea->scroll_top) < textarea->line_count; i++) {
        line = &textarea->lines[i + textarea->scroll_top];
        line_y = y + TEXTAREA_PADDING + i * line_height;
        
        /* Draw visible characters in this line */
        for (j = textarea->scroll_left; j < line->length && j < (textarea->scroll_left + textarea->visible_cols); j++) {
            char_x = x + TEXTAREA_PADDING + (j - textarea->scroll_left) * char_width;
            if (textarea->edit_base.font == FONT_9X16) {
                /* Use BIOS font for 9x16 */
                dispi_draw_char_bios(char_x, line_y, line->text[j], text_color, bg_color);
            } else {
                dispi_draw_char(char_x, line_y, line->text[j], text_color, bg_color);
            }
        }
    }
    
    /* Draw cursor if focused */
    if (textarea->edit_base.has_focus) {
        int cursor_visible_line = textarea->cursor_line - textarea->scroll_top;
        int cursor_visible_col = textarea->cursor_col - textarea->scroll_left;
        
        /* Update cursor blink using shared base */
        text_edit_base_update_cursor(&textarea->edit_base);
        
        /* Draw cursor if visible and in view */
        if (textarea->edit_base.cursor_visible && 
            cursor_visible_line >= 0 && cursor_visible_line < textarea->visible_lines &&
            cursor_visible_col >= 0 && cursor_visible_col <= textarea->visible_cols) {
            
            int cursor_x = x + TEXTAREA_PADDING + cursor_visible_col * char_width;
            int cursor_y = y + TEXTAREA_PADDING + cursor_visible_line * line_height;
            
            /* Draw background-style cursor */
            gc_fill_rect(gc, cursor_x, cursor_y, char_width, char_height, textarea->edit_base.cursor_color);
            
            /* If there's a character at cursor position, redraw it in contrasting color */
            if (textarea->cursor_col < textarea->lines[textarea->cursor_line].length) {
                char c = textarea->lines[textarea->cursor_line].text[textarea->cursor_col];
                /* Draw black character on gold cursor background */
                if (textarea->edit_base.font == FONT_9X16) {
                    dispi_draw_char_bios(cursor_x, cursor_y, c, COLOR_BLACK, textarea->edit_base.cursor_color);
                } else {
                    dispi_draw_char(cursor_x, cursor_y, c, COLOR_BLACK, textarea->edit_base.cursor_color);
                }
            }
        }
    }
}

/* Event bus handler for keyboard events */
static int textarea_keyboard_handler(View *view, InputEvent *event, void *context) {
    TextArea *textarea = (TextArea*)context;
    
    if (!textarea || !event || event->type != EVENT_KEY_DOWN) {
        return 0;
    }
    
    /* Only handle if we have focus */
    if (!textarea->edit_base.has_focus) {
        return 0;
    }
    
    serial_write_string("TextArea: Handling keyboard event via event bus\n");
    
    /* Handle the key */
    textarea_handle_key(textarea, event->data.keyboard.ascii);
    
    /* Reset typing timer to keep cursor solid */
    text_edit_base_reset_typing_timer(&textarea->edit_base);
    
    view_invalidate((View*)textarea);
    
    return 1;  /* Event handled */
}

/* Event bus handler for mouse events */
static int textarea_mouse_handler(View *view, InputEvent *event, void *context) {
    TextArea *textarea = (TextArea*)context;
    int line_idx, col_idx;
    int local_x, local_y;
    RegionRect abs_bounds;
    int abs_x, abs_y;
    
    if (!textarea || !event) {
        return 0;
    }
    
    if (event->type == EVENT_MOUSE_DOWN) {
        /* Check if click is within our bounds */
        if (text_edit_base_hit_test((View*)textarea, event->data.mouse.x, event->data.mouse.y)) {
            /* Focus if not already focused */
            if (!textarea->edit_base.has_focus) {
                text_edit_base_set_focus(&textarea->edit_base, (View*)textarea, 1);
            }
            
            /* Get absolute position for cursor calculation */
            view_get_absolute_bounds((View*)textarea, &abs_bounds);
            grid_region_to_pixel(abs_bounds.x, abs_bounds.y, &abs_x, &abs_y);
            
            /* Calculate local coordinates relative to textarea content area */
            local_x = event->data.mouse.x - abs_x;
            local_y = event->data.mouse.y - abs_y;
            
            /* Calculate which line was clicked */
            line_idx = get_line_at_y(textarea, local_y);
            if (line_idx >= 0 && line_idx < textarea->line_count) {
                textarea->cursor_line = line_idx;
                
                /* Calculate column within line */
                col_idx = get_col_at_x(textarea, line_idx, local_x);
                textarea->cursor_col = col_idx;
            }
            
            /* Reset cursor blink using shared base */
            text_edit_base_reset_typing_timer(&textarea->edit_base);
            
            serial_write_string("TextArea: Handling mouse click via event bus\n");
            view_invalidate((View*)textarea);
            return 1;
        } else {
            /* Click outside - lose focus */
            if (textarea->edit_base.has_focus) {
                text_edit_base_set_focus(&textarea->edit_base, (View*)textarea, 0);
                view_invalidate((View*)textarea);
            }
        }
    }
    
    return 0;
}

/* Handle events (legacy - kept for compatibility) */
static int textarea_handle_event(View *view, InputEvent *event) {
    TextArea *textarea = (TextArea*)view;
    int line_idx, col_idx;
    int local_x, local_y;
    RegionRect abs_bounds;
    int abs_x, abs_y;
    int handled;
    
    switch (event->type) {
        case EVENT_MOUSE_DOWN:
            /* Use shared base mouse handling */
            handled = text_edit_base_handle_mouse_down(&textarea->edit_base, view, event);
            
            if (!handled) {
                return 0;  /* Click was outside, focus already handled */
            }
            
            /* Get absolute position for cursor calculation */
            view_get_absolute_bounds(view, &abs_bounds);
            grid_region_to_pixel(abs_bounds.x, abs_bounds.y, &abs_x, &abs_y);
            
            /* Calculate local coordinates relative to textarea content area */
            local_x = event->data.mouse.x - abs_x;
            local_y = event->data.mouse.y - abs_y;
            
            /* Calculate which line was clicked */
            line_idx = get_line_at_y(textarea, local_y);
            if (line_idx >= 0 && line_idx < textarea->line_count) {
                textarea->cursor_line = line_idx;
                
                /* Calculate column within line */
                col_idx = get_col_at_x(textarea, line_idx, local_x);
                textarea->cursor_col = col_idx;
            }
            
            /* Reset cursor blink using shared base */
            text_edit_base_reset_typing_timer(&textarea->edit_base);
            
            view->needs_redraw = 1;
            return 1;
            
        case EVENT_KEY_DOWN:
            if (textarea->edit_base.has_focus) {
                textarea_handle_key(textarea, event->data.keyboard.ascii);
                /* Reset typing timer to keep cursor solid */
                text_edit_base_reset_typing_timer(&textarea->edit_base);
                view->needs_redraw = 1;
                return 1;
            }
            break;
            
        default:
            break;
    }
    
    return 0;
}

/* Get line index at y coordinate */
static int get_line_at_y(TextArea *textarea, int y) {
    int line_height = (textarea->edit_base.font == FONT_9X16) ? LINE_HEIGHT_9X16 : LINE_HEIGHT_6X8;
    int relative_y = y - TEXTAREA_PADDING;
    int line_idx;
    
    if (relative_y < 0) return 0;
    
    line_idx = relative_y / line_height + textarea->scroll_top;
    if (line_idx >= textarea->line_count) {
        line_idx = textarea->line_count - 1;
    }
    
    return line_idx;
}

/* Get column index at x coordinate for given line */
static int get_col_at_x(TextArea *textarea, int line_idx, int x) {
    int char_width = (textarea->edit_base.font == FONT_9X16) ? 9 : 6;
    int relative_x = x - TEXTAREA_PADDING;
    int col_idx;
    
    if (relative_x < 0) return 0;
    
    col_idx = relative_x / char_width + textarea->scroll_left;
    if (col_idx > textarea->lines[line_idx].length) {
        col_idx = textarea->lines[line_idx].length;
    }
    
    return col_idx;
}

/* Insert character at cursor */
void textarea_insert_char(TextArea *textarea, char c) {
    TextAreaLine *line = &textarea->lines[textarea->cursor_line];
    int i;
    
    /* Handle newline */
    if (c == '\n' || c == '\r') {
        TextAreaLine *new_line;
        int remaining_len;
        
        /* Check if we can add a new line */
        if (textarea->line_count >= TEXTAREA_MAX_LINES) {
            return;
        }
        
        /* Shift lines down */
        for (i = textarea->line_count; i > textarea->cursor_line + 1; i--) {
            textarea->lines[i] = textarea->lines[i - 1];
        }
        
        /* Split current line at cursor */
        new_line = &textarea->lines[textarea->cursor_line + 1];
        remaining_len = line->length - textarea->cursor_col;
        
        /* Copy remaining text to new line */
        for (i = 0; i < remaining_len; i++) {
            new_line->text[i] = line->text[textarea->cursor_col + i];
        }
        new_line->text[remaining_len] = '\0';
        new_line->length = remaining_len;
        
        /* Truncate current line */
        line->text[textarea->cursor_col] = '\0';
        line->length = textarea->cursor_col;
        
        /* Move cursor to start of new line */
        textarea->line_count++;
        textarea->cursor_line++;
        textarea->cursor_col = 0;
        
    } else {
        /* Regular character insertion */
        if (line->length >= TEXTAREA_MAX_LINE_LENGTH - 1) {
            return;  /* Line too long */
        }
        
        /* Shift characters right */
        for (i = line->length; i > textarea->cursor_col; i--) {
            line->text[i] = line->text[i - 1];
        }
        
        /* Insert character */
        line->text[textarea->cursor_col] = c;
        line->length++;
        line->text[line->length] = '\0';
        textarea->cursor_col++;
        textarea->total_chars++;
    }
    
    /* Reset typing timer */
    text_edit_base_reset_typing_timer(&textarea->edit_base);
    
    /* Ensure cursor is visible */
    ensure_cursor_visible(textarea);
}

/* Delete character at cursor */
void textarea_delete_char(TextArea *textarea) {
    TextAreaLine *line = &textarea->lines[textarea->cursor_line];
    int i;
    
    if (textarea->cursor_col < line->length) {
        /* Delete within line */
        for (i = textarea->cursor_col; i < line->length - 1; i++) {
            line->text[i] = line->text[i + 1];
        }
        line->length--;
        line->text[line->length] = '\0';
        textarea->total_chars--;
        
    } else if (textarea->cursor_line < textarea->line_count - 1) {
        /* At end of line - merge with next line */
        TextAreaLine *next_line = &textarea->lines[textarea->cursor_line + 1];
        int space_left = TEXTAREA_MAX_LINE_LENGTH - 1 - line->length;
        int chars_to_copy = (next_line->length <= space_left) ? next_line->length : space_left;
        
        /* Append as much of next line as will fit */
        for (i = 0; i < chars_to_copy; i++) {
            line->text[line->length + i] = next_line->text[i];
        }
        line->length += chars_to_copy;
        line->text[line->length] = '\0';
        
        /* Shift remaining lines up */
        for (i = textarea->cursor_line + 1; i < textarea->line_count - 1; i++) {
            textarea->lines[i] = textarea->lines[i + 1];
        }
        textarea->line_count--;
    }
    
    text_edit_base_reset_typing_timer(&textarea->edit_base);
}

/* Backspace */
void textarea_backspace(TextArea *textarea) {
    TextAreaLine *line = &textarea->lines[textarea->cursor_line];
    int i;
    
    if (textarea->cursor_col > 0) {
        /* Delete within line */
        textarea->cursor_col--;
        for (i = textarea->cursor_col; i < line->length - 1; i++) {
            line->text[i] = line->text[i + 1];
        }
        line->length--;
        line->text[line->length] = '\0';
        textarea->total_chars--;
        
    } else if (textarea->cursor_line > 0) {
        /* At start of line - merge with previous line */
        TextAreaLine *prev_line = &textarea->lines[textarea->cursor_line - 1];
        int space_left = TEXTAREA_MAX_LINE_LENGTH - 1 - prev_line->length;
        int chars_to_copy = (line->length <= space_left) ? line->length : space_left;
        
        /* Move cursor to end of previous line */
        textarea->cursor_line--;
        textarea->cursor_col = prev_line->length;
        
        /* Append current line to previous */
        for (i = 0; i < chars_to_copy; i++) {
            prev_line->text[prev_line->length + i] = line->text[i];
        }
        prev_line->length += chars_to_copy;
        prev_line->text[prev_line->length] = '\0';
        
        /* Shift remaining lines up */
        for (i = textarea->cursor_line + 1; i < textarea->line_count - 1; i++) {
            textarea->lines[i] = textarea->lines[i + 1];
        }
        textarea->line_count--;
    }
    
    text_edit_base_reset_typing_timer(&textarea->edit_base);
    ensure_cursor_visible(textarea);
}

/* Move cursor up */
void textarea_move_cursor_up(TextArea *textarea) {
    if (textarea->cursor_line > 0) {
        textarea->cursor_line--;
        /* Adjust column if new line is shorter */
        if (textarea->cursor_col > textarea->lines[textarea->cursor_line].length) {
            textarea->cursor_col = textarea->lines[textarea->cursor_line].length;
        }
        ensure_cursor_visible(textarea);
    }
}

/* Move cursor down */
void textarea_move_cursor_down(TextArea *textarea) {
    if (textarea->cursor_line < textarea->line_count - 1) {
        textarea->cursor_line++;
        /* Adjust column if new line is shorter */
        if (textarea->cursor_col > textarea->lines[textarea->cursor_line].length) {
            textarea->cursor_col = textarea->lines[textarea->cursor_line].length;
        }
        ensure_cursor_visible(textarea);
    }
}

/* Move cursor left */
void textarea_move_cursor_left(TextArea *textarea) {
    if (textarea->cursor_col > 0) {
        textarea->cursor_col--;
    } else if (textarea->cursor_line > 0) {
        /* Move to end of previous line */
        textarea->cursor_line--;
        textarea->cursor_col = textarea->lines[textarea->cursor_line].length;
    }
    ensure_cursor_visible(textarea);
}

/* Move cursor right */
void textarea_move_cursor_right(TextArea *textarea) {
    TextAreaLine *line = &textarea->lines[textarea->cursor_line];
    
    if (textarea->cursor_col < line->length) {
        textarea->cursor_col++;
    } else if (textarea->cursor_line < textarea->line_count - 1) {
        /* Move to start of next line */
        textarea->cursor_line++;
        textarea->cursor_col = 0;
    }
    ensure_cursor_visible(textarea);
}

/* Move cursor to start of line */
void textarea_move_cursor_home(TextArea *textarea) {
    textarea->cursor_col = 0;
    ensure_cursor_visible(textarea);
}

/* Move cursor to end of line */
void textarea_move_cursor_end(TextArea *textarea) {
    textarea->cursor_col = textarea->lines[textarea->cursor_line].length;
    ensure_cursor_visible(textarea);
}

/* Page up - scroll up by visible lines */
void textarea_page_up(TextArea *textarea) {
    int i;
    for (i = 0; i < textarea->visible_lines && textarea->cursor_line > 0; i++) {
        textarea->cursor_line--;
    }
    /* Adjust column if new line is shorter */
    if (textarea->cursor_col > textarea->lines[textarea->cursor_line].length) {
        textarea->cursor_col = textarea->lines[textarea->cursor_line].length;
    }
    ensure_cursor_visible(textarea);
}

/* Page down - scroll down by visible lines */
void textarea_page_down(TextArea *textarea) {
    int i;
    for (i = 0; i < textarea->visible_lines && textarea->cursor_line < textarea->line_count - 1; i++) {
        textarea->cursor_line++;
    }
    /* Adjust column if new line is shorter */
    if (textarea->cursor_col > textarea->lines[textarea->cursor_line].length) {
        textarea->cursor_col = textarea->lines[textarea->cursor_line].length;
    }
    ensure_cursor_visible(textarea);
}

/* Delete word backward (Ctrl+W) */
void textarea_delete_word_backward(TextArea *textarea) {
    TextAreaLine *line = &textarea->lines[textarea->cursor_line];
    int start_col = textarea->cursor_col;
    int chars_to_delete;
    int i;
    
    if (textarea->cursor_col == 0) {
        /* At start of line - just do regular backspace */
        textarea_backspace(textarea);
        return;
    }
    
    /* Skip trailing spaces */
    while (textarea->cursor_col > 0 && line->text[textarea->cursor_col - 1] == ' ') {
        textarea->cursor_col--;
    }
    
    /* Delete word characters */
    while (textarea->cursor_col > 0 && line->text[textarea->cursor_col - 1] != ' ') {
        textarea->cursor_col--;
    }
    
    /* Remove characters from cursor_col to start_col */
    chars_to_delete = start_col - textarea->cursor_col;
    for (i = textarea->cursor_col; i < line->length - chars_to_delete; i++) {
        line->text[i] = line->text[i + chars_to_delete];
    }
    line->length -= chars_to_delete;
    line->text[line->length] = '\0';
    textarea->total_chars -= chars_to_delete;
    
    text_edit_base_reset_typing_timer(&textarea->edit_base);
}

/* Delete to end of line (Ctrl+K) */
void textarea_delete_to_end_of_line(TextArea *textarea) {
    TextAreaLine *line = &textarea->lines[textarea->cursor_line];
    
    if (textarea->cursor_col >= line->length) {
        /* At end of line - delete newline by merging with next line */
        if (textarea->cursor_line < textarea->line_count - 1) {
            textarea_delete_char(textarea);
        }
    } else {
        /* Delete from cursor to end of line */
        int chars_to_delete = line->length - textarea->cursor_col;
        textarea->total_chars -= chars_to_delete;
        line->length = textarea->cursor_col;
        line->text[line->length] = '\0';
    }
    
    text_edit_base_reset_typing_timer(&textarea->edit_base);
}

/* Delete to start of line (Ctrl+U) */
void textarea_delete_to_start_of_line(TextArea *textarea) {
    TextAreaLine *line = &textarea->lines[textarea->cursor_line];
    int chars_to_delete;
    int i;
    
    if (textarea->cursor_col == 0) {
        return;  /* Already at start */
    }
    
    /* Move remaining text to beginning */
    chars_to_delete = textarea->cursor_col;
    for (i = 0; i < line->length - chars_to_delete; i++) {
        line->text[i] = line->text[i + chars_to_delete];
    }
    line->length -= chars_to_delete;
    line->text[line->length] = '\0';
    textarea->total_chars -= chars_to_delete;
    
    /* Move cursor to beginning */
    textarea->cursor_col = 0;
    
    text_edit_base_reset_typing_timer(&textarea->edit_base);
}

/* Ensure cursor is visible by adjusting scroll */
static void ensure_cursor_visible(TextArea *textarea) {
    /* Vertical scroll */
    if (textarea->cursor_line < textarea->scroll_top) {
        textarea->scroll_top = textarea->cursor_line;
    } else if (textarea->cursor_line >= textarea->scroll_top + textarea->visible_lines) {
        textarea->scroll_top = textarea->cursor_line - textarea->visible_lines + 1;
    }
    
    /* Horizontal scroll */
    if (textarea->cursor_col < textarea->scroll_left) {
        textarea->scroll_left = textarea->cursor_col;
    } else if (textarea->cursor_col > textarea->scroll_left + textarea->visible_cols) {
        textarea->scroll_left = textarea->cursor_col - textarea->visible_cols;
    }
}

/* Handle keyboard input */
void textarea_handle_key(TextArea *textarea, unsigned char key) {
    /* Special keys */
    switch (key) {
        case 0x08:  /* Backspace */
            textarea_backspace(textarea);
            break;
            
        case 0x7F:  /* Delete */
            textarea_delete_char(textarea);
            break;
            
        case '\r':  /* Enter */
        case '\n':
            textarea_insert_char(textarea, '\n');
            break;
            
        case 0x1B:  /* ESC - could be used for vim mode later */
            break;
            
        /* Arrow keys (from extended scan codes) */
        case 0x11:  /* Up arrow */
            textarea_move_cursor_up(textarea);
            break;
            
        case 0x12:  /* Down arrow */
            textarea_move_cursor_down(textarea);
            break;
            
        case 0x13:  /* Left arrow */
            textarea_move_cursor_left(textarea);
            break;
            
        case 0x14:  /* Right arrow */
            textarea_move_cursor_right(textarea);
            break;
            
        case 0x15:  /* Home key */
            textarea_move_cursor_home(textarea);
            break;
            
        case 0x16:  /* End key */
            textarea_move_cursor_end(textarea);
            break;
            
        case 0x17:  /* Page Up */
            textarea_page_up(textarea);
            break;
            
        case 0x18:  /* Page Down */
            textarea_page_down(textarea);
            break;
            
        /* Ctrl combinations (still support these for compatibility) */
        case 0x01:  /* Ctrl-A - home */
            textarea_move_cursor_home(textarea);
            break;
            
        case 0x05:  /* Ctrl-E - end */
            textarea_move_cursor_end(textarea);
            break;
            
        case 0x02:  /* Ctrl-B - left (backwards) */
            textarea_move_cursor_left(textarea);
            break;
            
        case 0x06:  /* Ctrl-F - right (forward) */
            textarea_move_cursor_right(textarea);
            break;
            
        case 0x0E:  /* Ctrl-N - down (next) */
            textarea_move_cursor_down(textarea);
            break;
            
        case 0x10:  /* Ctrl-P - up (previous) */
            textarea_move_cursor_up(textarea);
            break;
            
        case 0x0B:  /* Ctrl-K - delete to end of line */
            textarea_delete_to_end_of_line(textarea);
            break;
            
        default:
            /* Regular character */
            if (key >= 32 && key < 127) {
                textarea_insert_char(textarea, key);
            }
            break;
    }
}

/* Set textarea text */
void textarea_set_text(TextArea *textarea, const char *text) {
    int i = 0;
    int line_idx = 0;
    int col_idx = 0;
    
    /* Clear existing text */
    textarea->line_count = 1;
    textarea->total_chars = 0;
    for (i = 0; i < TEXTAREA_MAX_LINES; i++) {
        textarea->lines[i].text[0] = '\0';
        textarea->lines[i].length = 0;
    }
    
    /* Parse text into lines */
    i = 0;
    while (text[i] != '\0' && line_idx < TEXTAREA_MAX_LINES) {
        if (text[i] == '\n' || text[i] == '\r') {
            /* End current line */
            textarea->lines[line_idx].text[col_idx] = '\0';
            textarea->lines[line_idx].length = col_idx;
            
            /* Start new line */
            line_idx++;
            col_idx = 0;
            if (line_idx < TEXTAREA_MAX_LINES) {
                textarea->line_count++;
            }
            
            /* Skip \r\n pairs */
            if (text[i] == '\r' && text[i + 1] == '\n') {
                i++;
            }
        } else if (col_idx < TEXTAREA_MAX_LINE_LENGTH - 1) {
            /* Add character to current line */
            textarea->lines[line_idx].text[col_idx] = text[i];
            col_idx++;
            textarea->total_chars++;
        }
        i++;
    }
    
    /* Finalize last line */
    textarea->lines[line_idx].text[col_idx] = '\0';
    textarea->lines[line_idx].length = col_idx;
    
    /* Reset cursor and scroll */
    textarea->cursor_line = 0;
    textarea->cursor_col = 0;
    textarea->scroll_top = 0;
    textarea->scroll_left = 0;
}

/* Get textarea text */
void textarea_get_text(TextArea *textarea, char *buffer, int buffer_size) {
    int i, j;
    int pos = 0;
    
    for (i = 0; i < textarea->line_count && pos < buffer_size - 1; i++) {
        TextAreaLine *line = &textarea->lines[i];
        
        /* Copy line text */
        for (j = 0; j < line->length && pos < buffer_size - 1; j++) {
            buffer[pos++] = line->text[j];
        }
        
        /* Add newline between lines (except after last line) */
        if (i < textarea->line_count - 1 && pos < buffer_size - 1) {
            buffer[pos++] = '\n';
        }
    }
    
    buffer[pos] = '\0';
}

/* Set colors */
void textarea_set_colors(TextArea *textarea, unsigned char bg, unsigned char text,
                        unsigned char cursor, unsigned char border, unsigned char focus_border) {
    textarea->edit_base.bg_color = bg;
    textarea->edit_base.text_color = text;
    textarea->edit_base.cursor_color = cursor;
    textarea->edit_base.border_color = border;
    textarea->edit_base.focus_border_color = focus_border;
}

/* Set font */
void textarea_set_font(TextArea *textarea, FontSize font) {
    textarea->edit_base.font = font;
    
    /* Recalculate visible area */
    if (font == FONT_9X16) {
        textarea->visible_lines = (textarea->pixel_height - TEXTAREA_PADDING * 2 - 2) / LINE_HEIGHT_9X16;
        textarea->visible_cols = (textarea->pixel_width - TEXTAREA_PADDING * 2) / 9;
    } else {
        textarea->visible_lines = (textarea->pixel_height - TEXTAREA_PADDING * 2 - 2) / LINE_HEIGHT_6X8;
        textarea->visible_cols = (textarea->pixel_width - TEXTAREA_PADDING * 2) / 6;
    }
}

/* Set focus */
void textarea_set_focus(TextArea *textarea, int focus) {
    if (!textarea) return;
    
    /* Use interface notification if available */
    if (textarea->base.interface) {
        if (focus) {
            view_interface_notify_focus_gained(&textarea->base);
        } else {
            view_interface_notify_focus_lost(&textarea->base);
        }
    } else {
        /* Fall back to direct method */
        if (focus) {
            serial_write_string("TextArea: Got focus!\n");
        } else {
            serial_write_string("TextArea: Lost focus!\n");
        }
        text_edit_base_set_focus(&textarea->edit_base, &textarea->base, focus);
    }
}

/* Destroy textarea */
static void textarea_destroy(View *view) {
    /* TextArea will be freed by caller */
}