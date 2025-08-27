#ifndef LAYOUT_H
#define LAYOUT_H

#include "view.h"
#include "grid.h"

/* Region roles for Acme-style navigation */
typedef enum {
    REGION_STANDALONE,   /* Independent region */
    REGION_NAVIGATOR,    /* Controls another region (left pane) */
    REGION_TARGET        /* Controlled by a navigator (right pane) */
} RegionRole;

/* Layout types */
typedef enum {
    LAYOUT_SINGLE,      /* Single view fills screen */
    LAYOUT_SPLIT,       /* Split into navigator/target */
    LAYOUT_CUSTOM       /* Custom region arrangement */
} LayoutType;

/* Region - a rectangular area of the screen that contains a view */
typedef struct Region {
    /* Grid position (0-6, 0-5) */
    int x, y;
    int width, height;  /* In region units */
    
    /* Content */
    View *content;
    
    /* Relationships */
    RegionRole role;
    struct Region *controls;  /* For navigator->target relationship */
    struct Region *controlled_by;  /* For target->navigator relationship */
    
    /* Properties */
    int active;  /* Currently focused region */
    unsigned char border_color;
} Region;

/* Bar - vertical divider that can be positioned between regions */
typedef struct Bar {
    int position;  /* Column position (0-7), -1 for hidden */
    int visible;
    unsigned char color;
    View *content;  /* Optional content view for the bar */
} Bar;

/* Layout - manages the overall screen layout */
typedef struct Layout {
    /* Screen is divided into 7x6 regions */
    Region regions[6][7];  /* [row][col] */
    
    /* Vertical bar */
    Bar bar;
    
    /* Layout configuration */
    LayoutType type;
    
    /* Active region tracking */
    Region *active_region;
    View *focus_view;  /* Currently focused view */
    
    /* Root view for the entire layout */
    View *root_view;
    
    /* Layout properties */
    int needs_redraw;
    unsigned char background_color;
} Layout;

/* Layout initialization and configuration */
Layout* layout_create(void);
void layout_destroy(Layout *layout);
void layout_init(Layout *layout);
void layout_reset(Layout *layout);

/* Layout types */
void layout_set_single(Layout *layout, View *content);
void layout_set_split(Layout *layout, View *navigator, View *target, int split_column);
void layout_set_type(Layout *layout, LayoutType type);

/* Region management */
Region* layout_get_region(Layout *layout, int x, int y);
void layout_set_region_content(Layout *layout, int x, int y, int width, int height, View *content);
void layout_clear_region(Layout *layout, int x, int y, int width, int height);
void layout_merge_regions(Layout *layout, int x, int y, int width, int height);

/* Navigator/Target relationships */
void layout_link_navigator(Layout *layout, Region *navigator, Region *target);
void layout_unlink_navigator(Layout *layout, Region *navigator);

/* Bar management */
void layout_set_bar_position(Layout *layout, int position);
void layout_show_bar(Layout *layout, int show);
void layout_set_bar_content(Layout *layout, View *content);
int layout_get_bar_column(Layout *layout);

/* Focus management */
void layout_set_active_region(Layout *layout, Region *region);
void layout_focus_view(Layout *layout, View *view);
void layout_move_focus(Layout *layout, int direction);  /* 0=up, 1=right, 2=down, 3=left */
Region* layout_get_active_region(Layout *layout);
View* layout_get_focus_view(Layout *layout);

/* Drawing */
void layout_draw(Layout *layout, GraphicsContext *gc);
void layout_draw_regions(Layout *layout, GraphicsContext *gc);
void layout_draw_bar(Layout *layout, GraphicsContext *gc);
void layout_invalidate(Layout *layout);

/* Event handling */
int layout_handle_event(Layout *layout, InputEvent *event);
Region* layout_hit_test_region(Layout *layout, int pixel_x, int pixel_y);

/* Utility functions */
void layout_region_to_pixels(Region *region, int *x, int *y, int *w, int *h);
void layout_pixels_to_region(Layout *layout, int pixel_x, int pixel_y, int *reg_x, int *reg_y);
int layout_is_region_visible(Layout *layout, Region *region);

#endif /* LAYOUT_H */