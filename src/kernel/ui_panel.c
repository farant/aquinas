/* Panel Component Implementation */

#include "ui_panel.h"
#include "view_interface.h"
#include "graphics_context.h"
#include "grid.h"
#include "dispi.h"
#include "memory.h"
#include "serial.h"

/* Forward declarations for interface callbacks */
static void panel_interface_init(View *view, ViewContext *context);
static void panel_interface_destroy(View *view);
static int panel_interface_can_focus(View *view);
static RegionRect panel_interface_get_preferred_size(View *view);

/* Panel ViewInterface definition */
static ViewInterface panel_interface = {
    /* Lifecycle methods */
    panel_interface_init,
    panel_interface_destroy,
    
    /* Parent-child callbacks */
    NULL,  /* Use defaults - panels are containers */
    NULL,
    NULL,
    NULL,
    
    /* State changes */
    NULL,  /* Panels don't typically get focus */
    NULL,
    NULL,  /* Use default for visibility */
    NULL,  /* Use default for enabled */
    
    /* Capabilities */
    panel_interface_can_focus,
    panel_interface_get_preferred_size
};

/* Draw 3D border effect */
static void draw_3d_border(GraphicsContext *gc, int x, int y, int w, int h, BorderStyle style) {
    switch (style) {
        case BORDER_RAISED:
            /* Light on top/left, dark on bottom/right */
            gc_draw_line(gc, x, y, x + w - 1, y, COLOR_WHITE);           /* Top */
            gc_draw_line(gc, x, y, x, y + h - 1, COLOR_WHITE);           /* Left */
            gc_draw_line(gc, x + w - 1, y + 1, x + w - 1, y + h - 1, COLOR_DARK_GRAY); /* Right */
            gc_draw_line(gc, x + 1, y + h - 1, x + w - 1, y + h - 1, COLOR_DARK_GRAY); /* Bottom */
            /* Inner border for stronger effect */
            gc_draw_line(gc, x + 1, y + 1, x + w - 2, y + 1, COLOR_LIGHT_GRAY);  /* Inner top */
            gc_draw_line(gc, x + 1, y + 1, x + 1, y + h - 2, COLOR_LIGHT_GRAY);  /* Inner left */
            break;
            
        case BORDER_SUNKEN:
            /* Dark on top/left, light on bottom/right */
            gc_draw_line(gc, x, y, x + w - 1, y, COLOR_DARK_GRAY);       /* Top */
            gc_draw_line(gc, x, y, x, y + h - 1, COLOR_DARK_GRAY);       /* Left */
            gc_draw_line(gc, x + w - 1, y + 1, x + w - 1, y + h - 1, COLOR_WHITE);     /* Right */
            gc_draw_line(gc, x + 1, y + h - 1, x + w - 1, y + h - 1, COLOR_WHITE);     /* Bottom */
            /* Inner border */
            gc_draw_line(gc, x + 1, y + 1, x + w - 2, y + 1, COLOR_MED_DARK_GRAY);  /* Inner top */
            gc_draw_line(gc, x + 1, y + 1, x + 1, y + h - 2, COLOR_MED_DARK_GRAY);  /* Inner left */
            break;
            
        case BORDER_FLAT:
            /* Simple single-line border */
            gc_draw_rect(gc, x, y, w - 1, h - 1, COLOR_MED_DARK_GRAY);
            break;
            
        case BORDER_NONE:
        default:
            break;
    }
}

/* Draw panel */
void panel_draw(View *self, GraphicsContext *gc) {
    Panel *panel = (Panel*)self;
    RegionRect abs_bounds;
    int x, y, w, h;
    int title_height = 0;
    View *child;
    
    /* Get absolute bounds in pixels */
    view_get_absolute_bounds(self, &abs_bounds);
    grid_region_to_pixel(abs_bounds.x, abs_bounds.y, &x, &y);
    w = abs_bounds.width * REGION_WIDTH;
    h = abs_bounds.height * REGION_HEIGHT;
    
    /* Fill background */
    gc_fill_rect(gc, x, y, w, h, panel->bg_color);
    
    /* Draw border */
    if (panel->border_style != BORDER_NONE) {
        draw_3d_border(gc, x, y, w, h, panel->border_style);
    }
    
    /* Draw title if present */
    if (panel->title) {
        int char_width = (panel->title_font == FONT_9X16) ? 9 : 6;
        int char_height = (panel->title_font == FONT_9X16) ? 16 : 8;
        int title_len = 0;
        const char *p = panel->title;
        int title_x, title_y;
        int title_bg_height = char_height + 4;
        
        /* Count title length */
        while (*p) {
            title_len++;
            p++;
        }
        
        /* Draw title bar background */
        gc_fill_rect(gc, x + 2, y + 2, w - 4, title_bg_height, COLOR_MED_GRAY);
        
        /* Draw title text (centered) */
        title_x = x + (w - title_len * char_width) / 2;
        title_y = y + 2;
        
        if (panel->title_font == FONT_9X16) {
            dispi_draw_string_bios(title_x, title_y, panel->title, COLOR_BLACK, COLOR_MED_GRAY);
        } else {
            dispi_draw_string(title_x, title_y, panel->title, COLOR_BLACK, COLOR_MED_GRAY);
        }
        
        /* Draw separator line under title */
        gc_draw_line(gc, x + 2, y + title_bg_height + 2, x + w - 3, y + title_bg_height + 2, COLOR_MED_DARK_GRAY);
        
        title_height = title_bg_height + 4;
    }
    
    /* Draw children (they'll be clipped to panel bounds) */
    child = self->children;
    while (child) {
        /* Children draw themselves, but we could adjust their clip region here */
        child = child->next_sibling;
    }
}

/* ViewInterface callback implementations */

static void panel_interface_init(View *view, ViewContext *context) {
    Panel *panel = (Panel*)view;
    (void)context;  /* Unused for now */
    
    serial_write_string("Panel: Interface init called\n");
    
    /* Initialize panel defaults */
    panel->bg_color = THEME_BG;
    panel->border_style = BORDER_NONE;
}

static void panel_interface_destroy(View *view) {
    Panel *panel = (Panel*)view;
    
    serial_write_string("Panel: Interface destroy called\n");
    
    /* Clean up if needed */
    (void)panel;
}

static int panel_interface_can_focus(View *view) {
    /* Panels don't receive focus, they're just containers */
    (void)view;
    return 0;
}

static RegionRect panel_interface_get_preferred_size(View *view) {
    /* Return current bounds as preferred size */
    return view->bounds;
}

/* Create a panel */
Panel* panel_create(int x, int y, int width, int height) {
    Panel *panel;
    int region_w, region_h;
    
    panel = (Panel*)malloc(sizeof(Panel));
    if (!panel) return NULL;
    
    /* Convert to regions */
    region_w = (width + REGION_WIDTH - 1) / REGION_WIDTH;
    region_h = (height + REGION_HEIGHT - 1) / REGION_HEIGHT;
    
    /* Initialize base view */
    panel->base.bounds.x = x;
    panel->base.bounds.y = y;
    panel->base.bounds.width = region_w;
    panel->base.bounds.height = region_h;
    panel->base.parent = NULL;
    panel->base.children = NULL;
    panel->base.next_sibling = NULL;
    panel->base.visible = 1;
    panel->base.needs_redraw = 1;
    panel->base.z_order = 0;
    panel->base.user_data = NULL;
    panel->base.draw = panel_draw;
    panel->base.update = NULL;
    panel->base.handle_event = NULL;  /* Events pass through to children */
    panel->base.destroy = NULL;
    panel->base.type_name = "Panel";
    panel->base.interface = &panel_interface;  /* Set ViewInterface */
    
    /* Initialize the view through its interface */
    if (panel->base.interface) {
        ViewContext context = {NULL, NULL, NULL, NULL};
        view_interface_init(&panel->base, panel->base.interface, &context);
    }
    
    /* Initialize panel specific */
    panel->title = NULL;
    panel->title_font = FONT_6X8;
    panel->border_style = BORDER_RAISED;
    panel->bg_color = THEME_PANEL_BG;
    panel->border_color = THEME_BORDER;
    panel->padding = PADDING_MEDIUM;
    
    return panel;
}

/* Destroy a panel */
void panel_destroy(Panel *panel) {
    if (panel) {
        /* Destroy all children first */
        View *child = panel->base.children;
        while (child) {
            View *next = child->next_sibling;
            if (child->destroy) {
                child->destroy(child);
            }
            child = next;
        }
        free(panel);
    }
}

/* Set panel title */
void panel_set_title(Panel *panel, const char *title, FontSize font) {
    if (panel) {
        panel->title = title;
        panel->title_font = font;
        view_invalidate((View*)panel);
    }
}

/* Set border style */
void panel_set_border(Panel *panel, BorderStyle style, unsigned char color) {
    if (panel) {
        panel->border_style = style;
        panel->border_color = color;
        view_invalidate((View*)panel);
    }
}

/* Set background color */
void panel_set_background(Panel *panel, unsigned char color) {
    if (panel && panel->bg_color != color) {
        panel->bg_color = color;
        view_invalidate((View*)panel);
    }
}

/* Add child to panel */
void panel_add_child(Panel *panel, View *child) {
    if (panel && child) {
        /* Adjust child position for panel padding */
        /* Note: This is simplified - in practice we'd want layout management */
        view_add_child((View*)panel, child);
    }
}