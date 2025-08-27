#ifndef VIEW_H
#define VIEW_H

#include "grid.h"
#include "graphics_context.h"

/* Forward declarations */
struct View;
struct InputEvent;

/* Input event types and structure */
typedef enum {
    EVENT_KEY_DOWN,
    EVENT_KEY_UP,
    EVENT_MOUSE_DOWN,
    EVENT_MOUSE_UP,
    EVENT_MOUSE_MOVE,
    EVENT_MOUSE_CLICK
} EventType;

typedef struct InputEvent {
    EventType type;
    union {
        struct {
            int key;
            char ascii;
            int shift;
            int ctrl;
        } keyboard;
        struct {
            int x, y;
            int button;  /* 1=left, 2=right, 3=middle */
        } mouse;
    } data;
} InputEvent;

/* View structure - represents a drawable, interactive UI element */
typedef struct View {
    /* Position and size */
    RegionRect bounds;
    
    /* View hierarchy */
    struct View *parent;
    struct View *children;
    struct View *next_sibling;
    
    /* View properties */
    int visible;
    int needs_redraw;
    int z_order;  /* Higher values drawn on top */
    
    /* View state */
    void *user_data;  /* Application-specific data */
    
    /* View methods (virtual functions) */
    void (*draw)(struct View *self, GraphicsContext *gc);
    void (*update)(struct View *self, int delta_ms);
    int (*handle_event)(struct View *self, InputEvent *event);
    void (*destroy)(struct View *self);
    
    /* Optional: view type identifier for debugging */
    const char *type_name;
} View;

/* View lifecycle functions */
View* view_create(int x, int y, int width, int height);
void view_destroy(View *view);

/* View hierarchy management */
void view_add_child(View *parent, View *child);
void view_remove_child(View *parent, View *child);
View* view_get_root(View *view);

/* View properties */
void view_set_visible(View *view, int visible);
void view_set_bounds(View *view, int x, int y, int width, int height);
void view_bring_to_front(View *view);
void view_send_to_back(View *view);

/* View rendering */
void view_invalidate(View *view);
void view_invalidate_rect(View *view, RegionRect *rect);
void view_draw_tree(View *root, GraphicsContext *gc);
void view_draw(View *view, GraphicsContext *gc);

/* View updates */
void view_update_tree(View *root, int delta_ms);

/* Hit testing and event handling */
View* view_hit_test(View *root, int x, int y);
int view_handle_event(View *view, InputEvent *event);
void view_broadcast_event(View *root, InputEvent *event);

/* Utility functions */
void view_screen_to_local(View *view, int screen_x, int screen_y, int *local_x, int *local_y);
void view_local_to_screen(View *view, int local_x, int local_y, int *screen_x, int *screen_y);
int view_contains_point(View *view, int x, int y);
void view_get_absolute_bounds(View *view, RegionRect *abs_bounds);

#endif /* VIEW_H */