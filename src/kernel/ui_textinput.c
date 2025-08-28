/* Text Input Component Implementation */

#include "ui_textinput.h"
#include "text_edit_base.h"
#include "view_interface.h"
#include "graphics_context.h"
#include "grid.h"
#include "dispi.h"
#include "memory.h"
#include "serial.h"

#define CURSOR_BLINK_RATE 500  /* Milliseconds */
#define DEFAULT_BUFFER_SIZE 256

/* Forward declarations for interface callbacks */
static void textinput_interface_init(View *view, ViewContext *context);
static void textinput_interface_destroy(View *view);
static void textinput_interface_on_focus_gained(View *view);
static void textinput_interface_on_focus_lost(View *view);
static int textinput_interface_can_focus(View *view);
static RegionRect textinput_interface_get_preferred_size(View *view);

/* TextInput ViewInterface definition */
static ViewInterface textinput_interface = {
    /* Lifecycle methods */
    textinput_interface_init,
    textinput_interface_destroy,
    
    /* Parent-child callbacks */
    NULL,  /* Use defaults */
    NULL,
    NULL,
    NULL,
    
    /* State changes */
    textinput_interface_on_focus_gained,
    textinput_interface_on_focus_lost,
    NULL,  /* Use default for visibility */
    NULL,  /* Use default for enabled */
    
    /* Capabilities */
    textinput_interface_can_focus,
    textinput_interface_get_preferred_size
};

/* Calculate text input width in pixels */
static int calculate_input_width(int width) {
    /* Width is given in approximate characters, convert to pixels */
    return width * 6 + PADDING_MEDIUM * 2;
}

/* Calculate text input height based on font */
static int calculate_input_height(FontSize font) {
    int char_height = (font == FONT_9X16) ? 16 : 8;
    return char_height + PADDING_MEDIUM * 2;
}

/* Draw cursor at current position */
static void draw_cursor(TextInput *input, GraphicsContext *gc, int x, int y) {
    int char_width = (input->edit_base.font == FONT_9X16) ? 9 : 6;
    int char_height = (input->edit_base.font == FONT_9X16) ? 16 : 8;
    int cursor_x, cursor_y;
    int visible_text_start = input->scroll_offset;
    int cursor_offset;
    char cursor_char;
    
    /* Calculate cursor position */
    cursor_offset = input->cursor_pos - visible_text_start;
    if (cursor_offset < 0) return; /* Cursor is scrolled off left */
    
    cursor_x = x + PADDING_SMALL + cursor_offset * char_width;
    cursor_y = y + PADDING_SMALL + 2;  /* Adjusted down by 2 pixels */
    
    if (input->edit_base.cursor_visible) {
        /* Option 1: Background highlight (like vim/terminal) */
        /* Draw filled rectangle behind the character at cursor position */
        gc_fill_rect(gc, cursor_x, cursor_y, char_width, char_height, input->edit_base.cursor_color);
        
        /* Redraw the character at cursor position in inverted color if there is one */
        if (input->cursor_pos < input->text_length) {
            cursor_char = input->buffer[input->cursor_pos];
            /* Use dispi_draw_char with inverted colors */
            dispi_draw_char(cursor_x, cursor_y, cursor_char, COLOR_BLACK, input->edit_base.cursor_color);
        }
        
        /* Option 2: Underscore cursor (uncomment to use this instead) */
        /*
        gc_draw_line(gc, cursor_x, cursor_y + char_height - 1, 
                     cursor_x + char_width - 1, cursor_y + char_height - 1, COLOR_BLACK);
        gc_draw_line(gc, cursor_x, cursor_y + char_height - 2, 
                     cursor_x + char_width - 1, cursor_y + char_height - 2, COLOR_BLACK);
        */
    }
}

/* Draw text input */
void textinput_draw(View *self, GraphicsContext *gc) {
    TextInput *input = (TextInput*)self;
    RegionRect abs_bounds;
    int x, y, w, h;
    unsigned char bg_color, fg_color, border_color;
    int char_width, char_height;
    int text_x, text_y;
    const char *display_text;
    char visible_buffer[80];  /* Temporary buffer for visible portion */
    int max_visible_chars;
    int visible_start, visible_len;
    int i;
    
    /* Get absolute position from parent hierarchy */
    view_get_absolute_bounds(self, &abs_bounds);
    grid_region_to_pixel(abs_bounds.x, abs_bounds.y, &x, &y);
    
    /* Use actual input pixel dimensions */
    w = input->pixel_width;
    h = input->pixel_height;
    
    /* Get colors from shared base */
    text_edit_base_get_colors(&input->edit_base, &bg_color, &fg_color, &border_color);
    
    /* Fill background */
    gc_fill_rect(gc, x, y, w, h, bg_color);
    
    /* Draw border (sunken effect for input field) */
    gc_draw_line(gc, x, y, x + w - 1, y, COLOR_DARK_GRAY);       /* Top */
    gc_draw_line(gc, x, y, x, y + h - 1, COLOR_DARK_GRAY);       /* Left */
    gc_draw_line(gc, x + w - 1, y + 1, x + w - 1, y + h - 1, COLOR_WHITE);  /* Right */
    gc_draw_line(gc, x + 1, y + h - 1, x + w - 1, y + h - 1, COLOR_WHITE);  /* Bottom */
    
    /* Draw focus highlight if focused */
    if (input->edit_base.has_focus) {
        gc_draw_rect(gc, x - 1, y - 1, w + 1, h + 1, input->edit_base.focus_border_color);
    }
    
    /* Calculate text dimensions */
    char_width = (input->edit_base.font == FONT_9X16) ? 9 : 6;
    char_height = (input->edit_base.font == FONT_9X16) ? 16 : 8;
    
    /* Calculate visible text area */
    max_visible_chars = (w - PADDING_MEDIUM * 2) / char_width;
    
    /* Determine what text to display */
    if (input->text_length == 0 && input->placeholder && 
        !input->edit_base.has_focus) {
        /* Show placeholder */
        display_text = input->placeholder;
        fg_color = COLOR_MED_DARK_GRAY;
        visible_start = 0;
        visible_len = 0;
        while (display_text[visible_len] && visible_len < max_visible_chars) {
            visible_len++;
        }
    } else {
        /* Show actual text with scrolling */
        visible_start = input->scroll_offset;
        visible_len = input->text_length - visible_start;
        if (visible_len > max_visible_chars) {
            visible_len = max_visible_chars;
        }
        
        /* Copy visible portion to temporary buffer */
        for (i = 0; i < visible_len; i++) {
            visible_buffer[i] = input->buffer[visible_start + i];
        }
        visible_buffer[visible_len] = '\0';
        display_text = visible_buffer;
    }
    
    /* Draw text */
    text_x = x + PADDING_SMALL;
    text_y = y + (h - char_height) / 2;
    
    if (input->edit_base.font == FONT_9X16) {
        dispi_draw_string_bios(text_x, text_y, display_text, fg_color, bg_color);
    } else {
        dispi_draw_string(text_x, text_y, display_text, fg_color, bg_color);
    }
    
    /* Draw cursor if focused */
    if (input->edit_base.has_focus && input->text_length > 0) {
        draw_cursor(input, gc, x, y);
    } else if (input->edit_base.has_focus && input->text_length == 0) {
        /* Draw cursor at start when empty - use same style as draw_cursor */
        if (input->edit_base.cursor_visible) {
            /* Background highlight style for empty field - use same positioning as draw_cursor */
            gc_fill_rect(gc, text_x, y + PADDING_SMALL + 2, char_width, char_height, input->edit_base.cursor_color);
            
            /* Underscore style (uncomment to use this instead) */
            /*
            gc_draw_line(gc, text_x, text_y + char_height - 1, 
                         text_x + char_width - 1, text_y + char_height - 1, COLOR_BLACK);
            gc_draw_line(gc, text_x, text_y + char_height - 2, 
                         text_x + char_width - 1, text_y + char_height - 2, COLOR_BLACK);
            */
        }
    }
}

/* Update text input (for cursor blinking) */
void textinput_update(View *self, int delta_ms) {
    TextInput *input = (TextInput*)self;
    
    /* Update cursor blinking timer */
    if (input->edit_base.has_focus) {
        input->edit_base.typing_timer += delta_ms;
        
        /* Only blink if not typing */
        if (input->edit_base.typing_timer > 500) {
            input->edit_base.cursor_blink_timer += delta_ms;
            if (input->edit_base.cursor_blink_timer >= CURSOR_BLINK_RATE) {
                text_edit_base_update_cursor(&input->edit_base);
                view_invalidate(self);
            }
        } else {
            /* Keep cursor solid while typing */
            if (!input->edit_base.cursor_visible) {
                input->edit_base.cursor_visible = 1;
                view_invalidate(self);
            }
        }
    }
}

/* Adjust scroll to keep cursor visible with context */
static void adjust_scroll(TextInput *input) {
    int char_width = (input->edit_base.font == FONT_9X16) ? 9 : 6;
    int max_visible_chars = (input->pixel_width - PADDING_SMALL * 2) / char_width;
    int min_context = 3;  /* Keep at least 3 chars visible before cursor when possible */
    
    /* If all text fits, reset scroll to 0 */
    if (input->text_length <= max_visible_chars) {
        input->scroll_offset = 0;
        return;
    }
    
    /* If cursor is off the left edge, scroll left with context */
    if (input->cursor_pos < input->scroll_offset) {
        input->scroll_offset = input->cursor_pos - min_context;
        if (input->scroll_offset < 0) {
            input->scroll_offset = 0;
        }
    }
    /* If cursor is off the right edge, scroll right with context */
    else if (input->cursor_pos >= input->scroll_offset + max_visible_chars) {
        input->scroll_offset = input->cursor_pos - max_visible_chars + 1;
        /* Try to keep some context before cursor */
        if (input->cursor_pos >= min_context) {
            int ideal_offset = input->cursor_pos - max_visible_chars + min_context;
            if (ideal_offset > input->scroll_offset) {
                input->scroll_offset = ideal_offset;
            }
        }
    }
    
    /* Ensure scroll doesn't go past end of text */
    if (input->scroll_offset > input->text_length - max_visible_chars) {
        input->scroll_offset = input->text_length - max_visible_chars;
    }
    if (input->scroll_offset < 0) {
        input->scroll_offset = 0;
    }
}

/* Insert character at cursor position */
static void insert_char(TextInput *input, char c) {
    int i;
    
    if (input->text_length >= input->buffer_size - 1) {
        return; /* Buffer full */
    }
    
    /* Shift text right from cursor position */
    for (i = input->text_length; i > input->cursor_pos; i--) {
        input->buffer[i] = input->buffer[i - 1];
    }
    
    /* Insert character */
    input->buffer[input->cursor_pos] = c;
    input->text_length++;
    input->cursor_pos++;
    input->buffer[input->text_length] = '\0';
    
    /* Reset typing timer for solid cursor */
    text_edit_base_reset_typing_timer(&input->edit_base);
    
    /* Adjust scroll to keep cursor visible */
    adjust_scroll(input);
    
    /* Fire change callback */
    if (input->on_change) {
        input->on_change(input, input->user_data);
    }
    
    view_invalidate((View*)input);
}

/* Delete character at cursor position */
static void delete_char(TextInput *input) {
    int i;
    
    if (input->cursor_pos >= input->text_length) {
        return; /* Nothing to delete */
    }
    
    /* Shift text left */
    for (i = input->cursor_pos; i < input->text_length - 1; i++) {
        input->buffer[i] = input->buffer[i + 1];
    }
    
    input->text_length--;
    input->buffer[input->text_length] = '\0';
    
    /* Reset typing timer for solid cursor */
    text_edit_base_reset_typing_timer(&input->edit_base);
    
    /* Fire change callback */
    if (input->on_change) {
        input->on_change(input, input->user_data);
    }
    
    view_invalidate((View*)input);
}

/* Delete character before cursor (backspace) */
static void backspace_char(TextInput *input) {
    if (input->cursor_pos == 0) {
        return; /* Nothing to delete */
    }
    
    input->cursor_pos--;
    delete_char(input);
    
    /* Reset typing timer for solid cursor (delete_char already does this but be explicit) */
    text_edit_base_reset_typing_timer(&input->edit_base);
    
    /* Adjust scroll to keep cursor visible with context */
    adjust_scroll(input);
}

/* Handle text input events */
int textinput_handle_event(View *self, InputEvent *event) {
    TextInput *input = (TextInput*)self;
    int char_width;
    
    if (input->edit_base.state == TEXT_STATE_DISABLED) {
        return 0;
    }
    
    switch (event->type) {
        case EVENT_MOUSE_DOWN:
            /* Focus on click */
            if (!input->edit_base.has_focus) {
                textinput_set_focused(input, 1);
                
                /* Find parent layout and set focus */
                {
                    View *root = view_get_root(self);
                    if (root && root->parent == NULL) {
                        /* This is the layout's root view, find the layout */
                        /* For now, we'll need to pass layout to components or find another way */
                        /* TODO: Better focus management */
                    }
                }
            }
            
            /* Position cursor at click location */
            char_width = (input->edit_base.font == FONT_9X16) ? 9 : 6;
            /* TODO: Calculate cursor position from mouse x */
            
            return 1;
            
        case EVENT_KEY_DOWN:
            if (!input->edit_base.has_focus) {
                return 0;
            }
            
            /* Handle special keys */
            switch (event->data.keyboard.key) {
                case 0x0E:  /* Backspace */
                    backspace_char(input);
                    return 1;
                    
                case 0x1C:  /* Enter */
                    if (input->on_submit) {
                        input->on_submit(input, input->user_data);
                    }
                    return 1;
                    
                case 0x4B:  /* Left arrow */
                    if (input->cursor_pos > 0) {
                        input->cursor_pos--;
                        adjust_scroll(input);
                        view_invalidate(self);
                    }
                    return 1;
                    
                case 0x4D:  /* Right arrow */
                    if (input->cursor_pos < input->text_length) {
                        input->cursor_pos++;
                        adjust_scroll(input);
                        view_invalidate(self);
                    }
                    return 1;
                    
                case 0x47:  /* Home */
                    input->cursor_pos = 0;
                    adjust_scroll(input);
                    view_invalidate(self);
                    return 1;
                    
                case 0x4F:  /* End */
                    input->cursor_pos = input->text_length;
                    adjust_scroll(input);
                    view_invalidate(self);
                    return 1;
                    
                case 0x53:  /* Delete */
                    delete_char(input);
                    adjust_scroll(input);
                    return 1;
                    
                default:
                    /* Insert printable character */
                    if (event->data.keyboard.ascii >= 32 && 
                        event->data.keyboard.ascii < 127) {
                        insert_char(input, event->data.keyboard.ascii);
                        return 1;
                    }
                    break;
            }
            break;
            
        default:
            break;
    }
    
    return 0;
}

/* ViewInterface callback implementations */

static void textinput_interface_init(View *view, ViewContext *context) {
    TextInput *input = (TextInput*)view;
    (void)context;  /* Unused for now */
    
    serial_write_string("TextInput: Interface init called\n");
    
    /* Initialize shared text editing base if not already done */
    text_edit_base_init(&input->edit_base);
}

static void textinput_interface_destroy(View *view) {
    TextInput *input = (TextInput*)view;
    
    serial_write_string("TextInput: Interface destroy called\n");
    
    /* Free text buffer */
    if (input->buffer) {
        /* Note: Our allocator doesn't support free yet */
        input->buffer = NULL;
    }
}

static void textinput_interface_on_focus_gained(View *view) {
    TextInput *input = (TextInput*)view;
    
    serial_write_string("TextInput: Got focus via interface!\n");
    
    /* Update focus state using shared base */
    text_edit_base_set_focus(&input->edit_base, view, 1);
    
    /* Mark for redraw */
    view->needs_redraw = 1;
}

static void textinput_interface_on_focus_lost(View *view) {
    TextInput *input = (TextInput*)view;
    
    serial_write_string("TextInput: Lost focus via interface!\n");
    
    /* Update focus state using shared base */
    text_edit_base_set_focus(&input->edit_base, view, 0);
    
    /* Mark for redraw */
    view->needs_redraw = 1;
}

static int textinput_interface_can_focus(View *view) {
    TextInput *input = (TextInput*)view;
    
    /* Can focus if not disabled */
    return input->edit_base.state != TEXT_STATE_DISABLED;
}

static RegionRect textinput_interface_get_preferred_size(View *view) {
    TextInput *input = (TextInput*)view;
    RegionRect size;
    
    /* Return current bounds as preferred size */
    size = view->bounds;
    
    return size;
}

/* Create a text input */
TextInput* textinput_create(int x, int y, int width, const char *placeholder, FontSize font) {
    TextInput *input;
    int pixel_width, pixel_height;
    int region_w, region_h;
    
    input = (TextInput*)malloc(sizeof(TextInput));
    if (!input) return NULL;
    
    /* Allocate text buffer */
    input->buffer = (char*)malloc(DEFAULT_BUFFER_SIZE);
    if (!input->buffer) {
        free(input);
        return NULL;
    }
    
    /* Calculate dimensions in pixels */
    pixel_width = calculate_input_width(width);
    pixel_height = calculate_input_height(font);
    
    /* Store pixel-precise bounds */
    input->pixel_x = x * REGION_WIDTH;
    input->pixel_y = y * REGION_HEIGHT;
    input->pixel_width = pixel_width;
    input->pixel_height = pixel_height;
    
    /* Convert to regions for view system */
    region_w = (pixel_width + REGION_WIDTH - 1) / REGION_WIDTH;
    region_h = (pixel_height + REGION_HEIGHT - 1) / REGION_HEIGHT;
    
    /* Initialize base view */
    input->base.bounds.x = x;
    input->base.bounds.y = y;
    input->base.bounds.width = region_w;
    input->base.bounds.height = region_h;
    input->base.parent = NULL;
    input->base.children = NULL;
    input->base.next_sibling = NULL;
    input->base.visible = 1;
    input->base.needs_redraw = 1;
    input->base.z_order = 0;
    input->base.user_data = NULL;
    input->base.draw = textinput_draw;
    input->base.update = textinput_update;
    input->base.handle_event = textinput_handle_event;
    input->base.destroy = NULL;
    input->base.type_name = "TextInput";
    input->base.interface = &textinput_interface;  /* Set ViewInterface */
    
    /* Initialize shared text editing base */
    text_edit_base_init(&input->edit_base);
    input->edit_base.font = font;
    
    /* Initialize the view through its interface */
    if (input->base.interface) {
        ViewContext context = {NULL, NULL, NULL, NULL};
        view_interface_init(&input->base, input->base.interface, &context);
    }
    
    /* Initialize text buffer */
    input->buffer[0] = '\0';
    input->buffer_size = DEFAULT_BUFFER_SIZE;
    input->text_length = 0;
    
    /* Initialize cursor and selection */
    input->cursor_pos = 0;
    input->selection_start = -1;
    input->selection_end = -1;
    
    /* Initialize display */
    input->scroll_offset = 0;
    input->placeholder = placeholder;
    
    /* Initialize callbacks */
    input->on_change = NULL;
    input->on_submit = NULL;
    input->user_data = NULL;
    
    return input;
}

/* Destroy text input */
void textinput_destroy(TextInput *input) {
    if (input) {
        if (input->buffer) {
            free(input->buffer);
        }
        free(input);
    }
}

/* Set text content */
void textinput_set_text(TextInput *input, const char *text) {
    int len = 0;
    
    if (!input || !text) return;
    
    /* Copy text to buffer */
    while (text[len] && len < input->buffer_size - 1) {
        input->buffer[len] = text[len];
        len++;
    }
    input->buffer[len] = '\0';
    input->text_length = len;
    
    /* Reset cursor to end */
    input->cursor_pos = len;
    input->scroll_offset = 0;
    
    /* Fire change callback */
    if (input->on_change) {
        input->on_change(input, input->user_data);
    }
    
    view_invalidate((View*)input);
}

/* Get text content */
const char* textinput_get_text(TextInput *input) {
    return input ? input->buffer : "";
}

/* Clear text */
void textinput_clear(TextInput *input) {
    if (!input) return;
    
    input->buffer[0] = '\0';
    input->text_length = 0;
    input->cursor_pos = 0;
    input->scroll_offset = 0;
    
    /* Fire change callback */
    if (input->on_change) {
        input->on_change(input, input->user_data);
    }
    
    view_invalidate((View*)input);
}

/* Set focus state */
void textinput_set_focused(TextInput *input, int focused) {
    if (!input) return;
    
    /* Use interface notification if available */
    if (input->base.interface) {
        if (focused) {
            view_interface_notify_focus_gained(&input->base);
        } else {
            view_interface_notify_focus_lost(&input->base);
        }
    } else {
        /* Fall back to direct method */
        text_edit_base_set_focus(&input->edit_base, (View*)input, focused);
    }
}

/* Set enabled state */
void textinput_set_enabled(TextInput *input, int enabled) {
    if (!input) return;
    
    if (enabled && input->edit_base.state == TEXT_STATE_DISABLED) {
        input->edit_base.state = TEXT_STATE_NORMAL;
    } else if (!enabled) {
        input->edit_base.state = TEXT_STATE_DISABLED;
    }
    
    view_invalidate((View*)input);
}

/* Set change callback */
void textinput_set_on_change(TextInput *input, 
                             void (*on_change)(TextInput*, void*), 
                             void *user_data) {
    if (!input) return;
    
    input->on_change = on_change;
    input->user_data = user_data;
}

/* Set submit callback */
void textinput_set_on_submit(TextInput *input,
                             void (*on_submit)(TextInput*, void*),
                             void *user_data) {
    if (!input) return;
    
    input->on_submit = on_submit;
    input->user_data = user_data;
}