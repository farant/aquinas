/* Layout and View System Demo
 * 
 * Demonstrates the view hierarchy and layout management system
 * with interactive examples.
 */

#include "layout_demo.h"
#include "layout.h"
#include "view.h"
#include "graphics_context.h"
#include "display_driver.h"
#include "dispi.h"
#include "dispi_cursor.h"
#include "grid.h"
#include "serial.h"
#include "timer.h"
#include "input.h"
#include "vga.h"
#include "graphics.h"
#include "memory.h"
#include "io.h"

/* Demo view types */
typedef struct {
    View base;
    unsigned char color;
    const char *label;
    int counter;
} ColoredView;

typedef struct {
    View base;
    int selected_item;
    const char *items[10];
    int item_count;
} ListView;

typedef struct {
    View base;
    const char *text;
    int blink_state;
    unsigned int last_blink;
} TextView;

/* Custom draw functions for demo views */
static void colored_view_draw(View *self, GraphicsContext *gc) {
    ColoredView *cv = (ColoredView*)self;
    RegionRect abs_bounds;
    int x, y, w, h;
    
    /* Get absolute bounds and convert to pixels */
    view_get_absolute_bounds(self, &abs_bounds);
    grid_region_to_pixel(abs_bounds.x, abs_bounds.y, &x, &y);
    w = abs_bounds.width * REGION_WIDTH;
    h = abs_bounds.height * REGION_HEIGHT;
    
    /* Fill with color */
    gc_fill_rect(gc, x, y, w, h, cv->color);
    
    /* Draw border */
    gc_draw_rect(gc, x, y, w-1, h-1, 0);  /* Black border */
    
    /* Draw label if present */
    if (cv->label) {
        /* Use white text on the view's background color */
        dispi_draw_string_bios(x + 10, y + 10, cv->label, 15, cv->color);
    }
    
    /* Always draw counter (even if 0) to show it's clickable */
    {
        char buf[20];
        int i = 0, temp = cv->counter;
        
        /* Simple integer to string conversion */
        buf[0] = 'C'; buf[1] = 'l'; buf[2] = 'i'; buf[3] = 'c'; buf[4] = 'k'; 
        buf[5] = 's'; buf[6] = ':'; buf[7] = ' ';
        i = 8;
        
        if (temp == 0) {
            buf[i++] = '0';
        } else {
            int start = i;
            int end;
            while (temp > 0) {
                buf[i++] = '0' + (temp % 10);
                temp /= 10;
            }
            /* Reverse the digits */
            end = i - 1;
            while (start < end) {
                char c = buf[start];
                buf[start] = buf[end];
                buf[end] = c;
                start++;
                end--;
            }
        }
        buf[i] = '\0';
        
        /* Use white text on the view's background color */
        dispi_draw_string_bios(x + 10, y + 30, buf, 15, cv->color);
    }
}

static void colored_view_update(View *self, int delta_ms) {
    /* Could animate colors or other properties here */
}

static int colored_view_handle_event(View *self, InputEvent *event) {
    ColoredView *cv = (ColoredView*)self;
    
    if (event->type == EVENT_MOUSE_DOWN) {
        /* Increment counter on click */
        cv->counter++;
        view_invalidate(self);
        
        serial_write_string("ColoredView clicked! Counter: ");
        serial_write_int(cv->counter);
        serial_write_string("\n");
        
        return 1;  /* Event handled */
    }
    
    return 0;
}

static void list_view_draw(View *self, GraphicsContext *gc) {
    ListView *lv = (ListView*)self;
    RegionRect abs_bounds;
    int x, y, w, h;
    int i, item_y;
    
    /* Get absolute bounds and convert to pixels */
    view_get_absolute_bounds(self, &abs_bounds);
    grid_region_to_pixel(abs_bounds.x, abs_bounds.y, &x, &y);
    w = abs_bounds.width * REGION_WIDTH;
    h = abs_bounds.height * REGION_HEIGHT;
    
    /* Fill background */
    gc_fill_rect(gc, x, y, w, h, 1);  /* Dark gray */
    
    /* Draw border */
    gc_draw_rect(gc, x, y, w-1, h-1, 5);  /* White border */
    
    /* Draw title */
    dispi_draw_string_bios(x + 5, y + 5, "Navigator", 15, 1);
    
    /* Draw items */
    item_y = y + 25;
    for (i = 0; i < lv->item_count && i < 10; i++) {
        unsigned char fg_color = (i == lv->selected_item) ? 11 : 15;  /* Gold if selected */
        unsigned char bg_color = (i == lv->selected_item) ? 0 : 1;
        
        /* Draw selection bar */
        if (i == lv->selected_item) {
            gc_fill_rect(gc, x + 2, item_y - 2, w - 4, 18, 0);
        }
        
        /* Draw item text */
        dispi_draw_string_bios(x + 10, item_y, lv->items[i], fg_color, bg_color);
        item_y += 20;
    }
}

static int list_view_handle_event(View *self, InputEvent *event) {
    ListView *lv = (ListView*)self;
    RegionRect abs_bounds;
    int x, y, item_y, i;
    
    if (event->type == EVENT_MOUSE_DOWN) {
        /* Get view bounds */
        view_get_absolute_bounds(self, &abs_bounds);
        grid_region_to_pixel(abs_bounds.x, abs_bounds.y, &x, &y);
        
        /* Calculate which item was clicked */
        item_y = y + 25;  /* Start position of items */
        for (i = 0; i < lv->item_count && i < 10; i++) {
            if (event->data.mouse.y >= item_y && event->data.mouse.y < item_y + 16) {
                /* This item was clicked */
                lv->selected_item = i;
                view_invalidate(self);
                
                serial_write_string("List item clicked: ");
                serial_write_string(lv->items[i]);
                serial_write_string("\n");
                
                return 1;
            }
            item_y += 16;
        }
    } else if (event->type == EVENT_KEY_DOWN) {
        if (event->data.keyboard.key == -1) {  /* Up arrow */
            if (lv->selected_item > 0) {
                lv->selected_item--;
                view_invalidate(self);
                return 1;
            }
        } else if (event->data.keyboard.key == -2) {  /* Down arrow */
            if (lv->selected_item < lv->item_count - 1) {
                lv->selected_item++;
                view_invalidate(self);
                return 1;
            }
        }
    }
    
    return 0;
}

static void text_view_draw(View *self, GraphicsContext *gc) {
    TextView *tv = (TextView*)self;
    RegionRect abs_bounds;
    int x, y, w, h;
    
    /* Get absolute bounds and convert to pixels */
    view_get_absolute_bounds(self, &abs_bounds);
    grid_region_to_pixel(abs_bounds.x, abs_bounds.y, &x, &y);
    w = abs_bounds.width * REGION_WIDTH;
    h = abs_bounds.height * REGION_HEIGHT;
    
    /* Fill background */
    gc_fill_rect(gc, x, y, w, h, 0);  /* Black */
    
    /* Draw border */
    gc_draw_rect(gc, x, y, w-1, h-1, 14);  /* Cyan border */
    
    /* Draw text content */
    if (tv->text) {
        dispi_draw_string(x + 10, y + 10, tv->text, 14, 0);
    }
    
    /* Draw blinking cursor */
    if (tv->blink_state) {
        gc_fill_rect(gc, x + 10 + 6 * 10, y + 10, 6, 8, 14);  /* Cyan cursor */
    }
}

static void text_view_update(View *self, int delta_ms) {
    TextView *tv = (TextView*)self;
    unsigned int current_time = get_ticks();
    
    /* Blink cursor every 500ms */
    if (current_time - tv->last_blink >= 500) {
        tv->blink_state = !tv->blink_state;
        tv->last_blink = current_time;
        view_invalidate(self);
    }
}

/* Helper to create a colored view */
static ColoredView* create_colored_view(int x, int y, int w, int h, unsigned char color, const char *label) {
    ColoredView *cv = (ColoredView*)malloc(sizeof(ColoredView));
    if (!cv) return NULL;
    
    /* Initialize base view */
    cv->base.bounds.x = x;
    cv->base.bounds.y = y;
    cv->base.bounds.width = w;
    cv->base.bounds.height = h;
    cv->base.parent = NULL;
    cv->base.children = NULL;
    cv->base.next_sibling = NULL;
    cv->base.visible = 1;
    cv->base.needs_redraw = 1;
    cv->base.z_order = 0;
    cv->base.user_data = NULL;
    cv->base.draw = colored_view_draw;
    cv->base.update = colored_view_update;
    cv->base.handle_event = colored_view_handle_event;
    cv->base.destroy = NULL;
    cv->base.type_name = "ColoredView";
    
    /* Initialize colored view specific */
    cv->color = color;
    cv->label = label;
    cv->counter = 0;
    
    return cv;
}

/* Helper to create a list view */
static ListView* create_list_view(int x, int y, int w, int h) {
    ListView *lv = (ListView*)malloc(sizeof(ListView));
    if (!lv) return NULL;
    
    /* Initialize base view */
    lv->base.bounds.x = x;
    lv->base.bounds.y = y;
    lv->base.bounds.width = w;
    lv->base.bounds.height = h;
    lv->base.parent = NULL;
    lv->base.children = NULL;
    lv->base.next_sibling = NULL;
    lv->base.visible = 1;
    lv->base.needs_redraw = 1;
    lv->base.z_order = 0;
    lv->base.user_data = NULL;
    lv->base.draw = list_view_draw;
    lv->base.update = NULL;
    lv->base.handle_event = list_view_handle_event;
    lv->base.destroy = NULL;
    lv->base.type_name = "ListView";
    
    /* Initialize list view specific */
    lv->selected_item = 0;
    lv->items[0] = "File Browser";
    lv->items[1] = "Commands";
    lv->items[2] = "Search";
    lv->items[3] = "Pages";
    lv->items[4] = "Settings";
    lv->item_count = 5;
    
    return lv;
}

/* Helper to create a text view */
static TextView* create_text_view(int x, int y, int w, int h, const char *text) {
    TextView *tv = (TextView*)malloc(sizeof(TextView));
    if (!tv) return NULL;
    
    /* Initialize base view */
    tv->base.bounds.x = x;
    tv->base.bounds.y = y;
    tv->base.bounds.width = w;
    tv->base.bounds.height = h;
    tv->base.parent = NULL;
    tv->base.children = NULL;
    tv->base.next_sibling = NULL;
    tv->base.visible = 1;
    tv->base.needs_redraw = 1;
    tv->base.z_order = 0;
    tv->base.user_data = NULL;
    tv->base.draw = text_view_draw;
    tv->base.update = text_view_update;
    tv->base.handle_event = NULL;
    tv->base.destroy = NULL;
    tv->base.type_name = "TextView";
    
    /* Initialize text view specific */
    tv->text = text;
    tv->blink_state = 1;
    tv->last_blink = get_ticks();
    
    return tv;
}

/* Main layout demo function */
void test_layout_demo(void) {
    Layout *layout;
    GraphicsContext *gc;
    DisplayDriver *driver;
    ColoredView *view1, *view2, *view3;
    ListView *navigator;
    TextView *content;
    View *child1, *child2;
    int running = 1;
    int key;
    unsigned int last_update = 0;
    unsigned char aquinas_palette[16][3];
    
    serial_write_string("Starting Layout and View Demo\n");
    
    /* Initialize grid system (required for DISPI) */
    grid_init();
    
    /* Save VGA state */
    serial_write_string("Saving VGA font...\n");
    save_vga_font();
    
    /* Initialize DISPI driver */
    serial_write_string("Getting DISPI driver...\n");
    driver = dispi_get_driver();
    if (!driver) {
        serial_write_string("ERROR: Failed to get DISPI driver\n");
        return;
    }
    serial_write_string("Got driver, setting as active...\n");
    display_set_driver(driver);
    
    /* Enable double buffering */
    if (!dispi_init_double_buffer()) {
        serial_write_string("WARNING: Double buffering failed\n");
    }
    
    /* Set up proper Aquinas color palette */
    /* Aquinas palette - 8-bit values that match mode 12h after >> 2 */
    /* Grayscale (0-5) */
    aquinas_palette[0][0] = 0x00; aquinas_palette[0][1] = 0x00; aquinas_palette[0][2] = 0x00;  /* Black */
    aquinas_palette[1][0] = 0x40; aquinas_palette[1][1] = 0x40; aquinas_palette[1][2] = 0x40;  /* Dark gray */
    aquinas_palette[2][0] = 0x80; aquinas_palette[2][1] = 0x80; aquinas_palette[2][2] = 0x80;  /* Medium dark gray */
    aquinas_palette[3][0] = 0xC0; aquinas_palette[3][1] = 0xC0; aquinas_palette[3][2] = 0xC0;  /* Medium gray */
    aquinas_palette[4][0] = 0xE0; aquinas_palette[4][1] = 0xE0; aquinas_palette[4][2] = 0xE0;  /* Light gray */
    aquinas_palette[5][0] = 0xFC; aquinas_palette[5][1] = 0xFC; aquinas_palette[5][2] = 0xFC;  /* White */
    
    /* Reds (6-8) */
    aquinas_palette[6][0] = 0x80; aquinas_palette[6][1] = 0x20; aquinas_palette[6][2] = 0x20;  /* Dark red */
    aquinas_palette[7][0] = 0xC0; aquinas_palette[7][1] = 0x30; aquinas_palette[7][2] = 0x30;  /* Medium red */
    aquinas_palette[8][0] = 0xFC; aquinas_palette[8][1] = 0x40; aquinas_palette[8][2] = 0x40;  /* Bright red */
    
    /* Golds (9-11) */
    aquinas_palette[9][0] = 0xA0; aquinas_palette[9][1] = 0x80; aquinas_palette[9][2] = 0x20;   /* Dark gold */
    aquinas_palette[10][0] = 0xE0; aquinas_palette[10][1] = 0xC0; aquinas_palette[10][2] = 0x40; /* Medium gold */
    aquinas_palette[11][0] = 0xFC; aquinas_palette[11][1] = 0xE0; aquinas_palette[11][2] = 0x60; /* Bright yellow-gold */
    
    /* Cyans (12-14) */
    aquinas_palette[12][0] = 0x20; aquinas_palette[12][1] = 0x80; aquinas_palette[12][2] = 0xA0; /* Dark cyan */
    aquinas_palette[13][0] = 0x40; aquinas_palette[13][1] = 0xC0; aquinas_palette[13][2] = 0xE0; /* Medium cyan */
    aquinas_palette[14][0] = 0x60; aquinas_palette[14][1] = 0xE0; aquinas_palette[14][2] = 0xFC; /* Bright cyan */
    
    /* Background (15) */
    aquinas_palette[15][0] = 0xB0; aquinas_palette[15][1] = 0xA0; aquinas_palette[15][2] = 0x80; /* Warm gray */
    
    /* Set the palette */
    if (driver->set_palette) {
        driver->set_palette(aquinas_palette);
    }
    
    /* Initialize mouse cursor for DISPI mode */
    dispi_cursor_init();
    dispi_cursor_show();
    
    /* Create graphics context */
    gc = gc_create(driver);
    if (!gc) {
        serial_write_string("ERROR: Failed to create graphics context\n");
        return;
    }
    
    /* Create layout */
    layout = layout_create();
    if (!layout) {
        serial_write_string("ERROR: Failed to create layout\n");
        gc_destroy(gc);
        return;
    }
    
    /* Demo 1: Split layout with navigator and content */
    serial_write_string("Demo 1: Split layout\n");
    
    /* Create navigator list view */
    navigator = create_list_view(0, 0, 2, 6);
    
    /* Create content area with colored views */
    view1 = create_colored_view(2, 0, 5, 2, 6, "Red Region");
    view2 = create_colored_view(2, 2, 5, 2, 9, "Gold Region");  
    view3 = create_colored_view(2, 4, 5, 2, 12, "Cyan Region");
    
    /* Add child views to demonstrate hierarchy */
    /* Children should be positioned relative to parent, not absolute */
    /* view1 is at (2,0), so child positions are offsets from that */
    child1 = (View*)create_colored_view(0, 0, 2, 1, 11, "Child 1");  /* Top-left of parent */
    child2 = (View*)create_colored_view(2, 1, 1, 1, 14, "Child 2");  /* Bottom-right area */
    view_add_child((View*)view1, child1);
    view_add_child((View*)view1, child2);
    
    /* Set up the layout */
    layout_set_region_content(layout, 0, 0, 2, 6, (View*)navigator);
    layout_set_region_content(layout, 2, 0, 5, 2, (View*)view1);
    layout_set_region_content(layout, 2, 2, 5, 2, (View*)view2);
    layout_set_region_content(layout, 2, 4, 5, 2, (View*)view3);
    
    /* Show bar at position 2 */
    layout_set_bar_position(layout, 2);
    layout_show_bar(layout, 1);
    
    /* Set active region */
    layout_set_active_region(layout, layout_get_region(layout, 0, 0));
    
    /* Initial draw */
    layout_draw(layout, gc);
    if (dispi_is_double_buffered()) {
        /* Draw cursor on backbuffer before flipping */
        dispi_cursor_show();
        dispi_flip_buffers();
    } else {
        /* Single buffered - just show cursor */
        dispi_cursor_show();
    }
    
    serial_write_string("Layout demo displayed. Use arrows to navigate, click views, ESC to exit\n");
    
    /* Main demo loop */
    last_update = get_ticks();
    while (running) {
        unsigned int current_time = get_ticks();
        int delta_ms = current_time - last_update;
        static int mouse_x = 320, mouse_y = 240;
        static unsigned char mouse_bytes[3];
        static int mouse_byte_num = 0;
        int need_redraw = 0;
        
        /* Update views */
        if (delta_ms > 16) {  /* ~60 FPS */
            view_update_tree(layout->root_view, delta_ms);
            last_update = current_time;
            /* Check if any views need redrawing */
            if (layout->root_view && layout->root_view->needs_redraw) {
                need_redraw = 1;
            }
        }
        
        /* Check for mouse input on COM1 */
        if (inb(0x3FD) & 0x01) {
            unsigned char data = inb(0x3F8);
            signed char dx, dy;
            
            /* Microsoft Serial Mouse protocol parsing */
            if (data & 0x40) {
                /* Start of packet (bit 6 set) */
                mouse_bytes[0] = data;
                mouse_byte_num = 1;
            } else if (mouse_byte_num > 0) {
                mouse_bytes[mouse_byte_num++] = data;
                
                if (mouse_byte_num == 3) {
                    /* Complete packet received */
                    dx = ((mouse_bytes[0] & 0x03) << 6) | (mouse_bytes[1] & 0x3F);
                    dy = ((mouse_bytes[0] & 0x0C) << 4) | (mouse_bytes[2] & 0x3F);
                    
                    /* Sign extend */
                    if (dx & 0x80) dx |= 0xFFFFFF00;
                    if (dy & 0x80) dy |= 0xFFFFFF00;
                    
                    /* Check if mouse actually moved */
                    if (dx != 0 || dy != 0) {
                        /* Update mouse position (2x speed) */
                        mouse_x += dx * 2;
                        mouse_y += dy * 2;  /* Don't invert Y */
                        
                        /* Clamp to screen bounds */
                        if (mouse_x < 0) mouse_x = 0;
                        if (mouse_x >= 640) mouse_x = 639;
                        if (mouse_y < 0) mouse_y = 0;
                        if (mouse_y >= 480) mouse_y = 479;
                        
                        /* Update cursor position - this redraws it at new location */
                        dispi_cursor_move(mouse_x, mouse_y);
                        
                        /* Force a redraw so cursor movement is visible */
                        need_redraw = 1;
                    }
                    
                    /* Handle mouse clicks */
                    /* Left button is bit 5, right button is bit 4 */
                    if (mouse_bytes[0] & 0x20) {  /* Left button */
                        InputEvent event;
                        event.type = EVENT_MOUSE_DOWN;
                        event.data.mouse.x = mouse_x;
                        event.data.mouse.y = mouse_y;
                        event.data.mouse.button = 1;
                        if (layout_handle_event(layout, &event)) {
                            /* Event was handled, need redraw */
                            need_redraw = 1;
                        }
                    }
                    
                    mouse_byte_num = 0;
                }
            }
        }
        
        /* Check for keyboard input */
        key = keyboard_check();
        if (key == 27) {  /* ESC */
            running = 0;
            serial_write_string("ESC pressed, exiting demo\n");
        } else if (key == '1') {
            /* Demo: Switch to single layout */
            serial_write_string("Switching to single layout\n");
            content = create_text_view(0, 0, 7, 6, "Full screen text view");
            layout_set_single(layout, (View*)content);
            need_redraw = 1;
        } else if (key == '2') {
            /* Demo: Switch back to split */
            serial_write_string("Switching back to split layout\n");
            layout_set_region_content(layout, 0, 0, 2, 6, (View*)navigator);
            layout_set_region_content(layout, 2, 0, 5, 6, (View*)view1);
            need_redraw = 1;
        } else if (key == -3 || key == -4) {  /* Left/Right arrows */
            /* Move focus between regions */
            layout_move_focus(layout, (key == -3) ? 3 : 1);  /* 3=left, 1=right */
            need_redraw = 1;
        } else if (key > 0) {
            /* Send key event to focused view */
            InputEvent event;
            event.type = EVENT_KEY_DOWN;
            event.data.keyboard.key = key;
            event.data.keyboard.ascii = (char)key;
            if (layout_handle_event(layout, &event)) {
                need_redraw = 1;
            }
        }
        
        /* Redraw if needed */
        if (need_redraw || (layout && layout->needs_redraw) || 
            (layout->root_view && layout->root_view->needs_redraw)) {
            /* Draw the layout */
            layout_draw(layout, gc);
            
            /* Ensure cursor is redrawn on top by hiding and showing it */
            /* This forces it to be drawn on the current backbuffer */
            dispi_cursor_hide();
            dispi_cursor_show();
            
            /* Now flip buffers to show everything */
            /* Even if double buffering failed, still try to flip */
            dispi_flip_buffers();
            
            need_redraw = 0;  /* Clear the flag */
        }
    }
    
    /* Cleanup */
    serial_write_string("Cleaning up layout demo\n");
    
    layout_destroy(layout);
    gc_destroy(gc);
    
    /* Cleanup double buffering */
    if (dispi_is_double_buffered()) {
        dispi_cleanup_double_buffer();
    }
    
    /* Return to text mode */
    dispi_disable();
    restore_dac_palette();
    set_mode_03h();
    restore_vga_font();
    vga_clear_screen();
    
    serial_write_string("Layout demo complete\n");
}