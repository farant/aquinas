/* Layout Manager Implementation
 * 
 * Manages the screen layout divided into a 7x6 grid of regions.
 * Supports navigator/target relationships for Acme-style interfaces
 * and a moveable vertical bar.
 */

#include "layout.h"
#include "memory.h"
#include "serial.h"
#include "display_driver.h"
#include "view_interface.h"

/* Create a new layout */
Layout* layout_create(void) {
    Layout *layout = (Layout*)malloc(sizeof(Layout));
    if (!layout) {
        serial_write_string("ERROR: Failed to allocate layout\n");
        return NULL;
    }
    
    layout_init(layout);
    return layout;
}

/* Initialize a layout structure */
void layout_init(Layout *layout) {
    int row, col;
    
    if (!layout) return;
    
    /* Initialize all regions */
    for (row = 0; row < 6; row++) {
        for (col = 0; col < 7; col++) {
            Region *region = &layout->regions[row][col];
            region->x = col;
            region->y = row;
            region->width = 1;
            region->height = 1;
            region->content = NULL;
            region->role = REGION_STANDALONE;
            region->controls = NULL;
            region->controlled_by = NULL;
            region->active = 0;
            region->border_color = 1;  /* Dark gray border */
        }
    }
    
    /* Initialize bar */
    layout->bar.position = -1;  /* Hidden by default */
    layout->bar.visible = 0;
    layout->bar.color = 5;  /* White */
    layout->bar.content = NULL;
    
    /* Initialize layout state */
    layout->type = LAYOUT_SINGLE;
    layout->active_region = &layout->regions[0][0];
    layout->focus_view = NULL;
    layout->hover_view = NULL;
    layout->root_view = view_create(0, 0, 7, 6);  /* Full screen */
    layout->needs_redraw = 1;
    layout->background_color = 0;  /* Black */
    
    /* Create event bus */
    layout->event_bus = event_bus_create();
    if (!layout->event_bus) {
        serial_write_string("WARNING: Failed to create event bus for layout\n");
    }
}

/* Destroy a layout */
void layout_destroy(Layout *layout) {
    int row, col;
    
    if (!layout) return;
    
    /* Destroy all region content views */
    for (row = 0; row < 6; row++) {
        for (col = 0; col < 7; col++) {
            if (layout->regions[row][col].content) {
                view_destroy(layout->regions[row][col].content);
            }
        }
    }
    
    /* Destroy bar content if any */
    if (layout->bar.content) {
        view_destroy(layout->bar.content);
    }
    
    /* Destroy root view */
    if (layout->root_view) {
        view_destroy(layout->root_view);
    }
    
    /* Destroy event bus */
    if (layout->event_bus) {
        event_bus_destroy(layout->event_bus);
    }
    
    /* Note: Memory not actually freed due to allocator limitations */
}

/* Reset layout to initial state */
void layout_reset(Layout *layout) {
    int row, col;
    
    if (!layout) return;
    
    /* Clear all regions */
    for (row = 0; row < 6; row++) {
        for (col = 0; col < 7; col++) {
            layout->regions[row][col].content = NULL;
            layout->regions[row][col].role = REGION_STANDALONE;
            layout->regions[row][col].controls = NULL;
            layout->regions[row][col].controlled_by = NULL;
            layout->regions[row][col].active = 0;
        }
    }
    
    /* Hide bar */
    layout->bar.position = -1;
    layout->bar.visible = 0;
    
    /* Reset state */
    layout->type = LAYOUT_SINGLE;
    layout->active_region = &layout->regions[0][0];
    layout->focus_view = NULL;
    layout->hover_view = NULL;
    layout->needs_redraw = 1;
}

/* Set single view layout */
void layout_set_single(Layout *layout, View *content) {
    if (!layout) return;
    
    layout_reset(layout);
    layout->type = LAYOUT_SINGLE;
    
    /* Set content to fill entire screen */
    if (content) {
        view_set_bounds(content, 0, 0, 7, 6);
        view_add_child(layout->root_view, content);
        
        /* Assign to all regions for hit testing */
        /* In single layout, the view spans all regions */
        layout->regions[0][0].content = content;
        layout->regions[0][0].width = 7;
        layout->regions[0][0].height = 6;
    }
    
    layout_invalidate(layout);
}

/* Set split layout with navigator and target */
void layout_set_split(Layout *layout, View *navigator, View *target, int split_column) {
    if (!layout || split_column < 1 || split_column > 6) return;
    
    layout_reset(layout);
    layout->type = LAYOUT_SPLIT;
    
    /* Set up navigator on left */
    if (navigator) {
        view_set_bounds(navigator, 0, 0, split_column, 6);
        view_add_child(layout->root_view, navigator);
        
        layout->regions[0][0].content = navigator;
        layout->regions[0][0].width = split_column;
        layout->regions[0][0].height = 6;
        layout->regions[0][0].role = REGION_NAVIGATOR;
    }
    
    /* Set up target on right */
    if (target) {
        view_set_bounds(target, split_column, 0, 7 - split_column, 6);
        view_add_child(layout->root_view, target);
        
        layout->regions[0][split_column].content = target;
        layout->regions[0][split_column].x = split_column;
        layout->regions[0][split_column].width = 7 - split_column;
        layout->regions[0][split_column].height = 6;
        layout->regions[0][split_column].role = REGION_TARGET;
    }
    
    /* Link navigator and target */
    if (navigator && target) {
        layout_link_navigator(layout, &layout->regions[0][0], &layout->regions[0][split_column]);
    }
    
    /* Optionally show bar at split position */
    layout->bar.position = split_column;
    layout->bar.visible = 1;
    
    layout_invalidate(layout);
}

/* Set layout type */
void layout_set_type(Layout *layout, LayoutType type) {
    if (!layout) return;
    
    layout->type = type;
    layout_invalidate(layout);
}

/* Get region at coordinates */
Region* layout_get_region(Layout *layout, int x, int y) {
    if (!layout || x < 0 || x >= 7 || y < 0 || y >= 6) {
        return NULL;
    }
    
    return &layout->regions[y][x];
}

/* Helper function to reinitialize a view tree with proper ViewContext */
static void reinit_view_tree_with_context(View *view, ViewContext *context) {
    View *child;
    
    if (!view || !context) return;
    
    /* Reinitialize this view if it has an interface */
    if (view->interface && view->interface->init) {
        view->interface->init(view, context);
    }
    
    /* Recursively reinitialize children */
    child = view->children;
    while (child) {
        reinit_view_tree_with_context(child, context);
        child = child->next_sibling;
    }
}

/* Set content for a region area */
void layout_set_region_content(Layout *layout, int x, int y, int width, int height, View *content) {
    Region *region;
    int row, col;
    ViewContext context;
    
    if (!layout || x < 0 || x + width > 7 || y < 0 || y + height > 6) {
        return;
    }
    
    /* Clear the area first */
    layout_clear_region(layout, x, y, width, height);
    
    /* Get the primary region */
    region = &layout->regions[y][x];
    
    /* Set up the region */
    region->content = content;
    region->width = width;
    region->height = height;
    
    /* Set content bounds and add to hierarchy */
    if (content) {
        view_set_bounds(content, x, y, width, height);
        view_add_child(layout->root_view, content);
        
        /* Reinitialize the view tree with proper context including event bus */
        context.layout = layout;
        context.event_bus = layout->event_bus;
        context.resources = NULL;  /* Not implemented yet */
        context.theme = NULL;      /* Not implemented yet */
        reinit_view_tree_with_context(content, &context);
    }
    
    /* Mark other regions in the area as occupied */
    for (row = y; row < y + height; row++) {
        for (col = x; col < x + width; col++) {
            if (row != y || col != x) {
                layout->regions[row][col].content = NULL;  /* Occupied by primary */
            }
        }
    }
    
    layout_invalidate(layout);
}

/* Clear a region area */
void layout_clear_region(Layout *layout, int x, int y, int width, int height) {
    int row, col;
    View *content;
    
    if (!layout || x < 0 || x + width > 7 || y < 0 || y + height > 6) {
        return;
    }
    
    /* Remove content from all regions in area */
    for (row = y; row < y + height; row++) {
        for (col = x; col < x + width; col++) {
            content = layout->regions[row][col].content;
            if (content && content->parent == layout->root_view) {
                view_remove_child(layout->root_view, content);
            }
            layout->regions[row][col].content = NULL;
            layout->regions[row][col].width = 1;
            layout->regions[row][col].height = 1;
        }
    }
    
    layout_invalidate(layout);
}

/* Link navigator and target regions */
void layout_link_navigator(Layout *layout, Region *navigator, Region *target) {
    if (!layout || !navigator || !target) return;
    
    /* Break existing links */
    layout_unlink_navigator(layout, navigator);
    if (target->controlled_by) {
        layout_unlink_navigator(layout, target->controlled_by);
    }
    
    /* Create new link */
    navigator->controls = target;
    navigator->role = REGION_NAVIGATOR;
    target->controlled_by = navigator;
    target->role = REGION_TARGET;
    
    layout_invalidate(layout);
}

/* Unlink navigator from its target */
void layout_unlink_navigator(Layout *layout, Region *navigator) {
    Region *target;
    
    if (!layout || !navigator) return;
    
    target = navigator->controls;
    if (target) {
        target->controlled_by = NULL;
        target->role = REGION_STANDALONE;
    }
    
    navigator->controls = NULL;
    navigator->role = REGION_STANDALONE;
    
    layout_invalidate(layout);
}

/* Set bar position */
void layout_set_bar_position(Layout *layout, int position) {
    if (!layout) return;
    
    if (position < 0 || position > 7) {
        position = -1;  /* Hide */
    }
    
    layout->bar.position = position;
    layout_invalidate(layout);
}

/* Show or hide bar */
void layout_show_bar(Layout *layout, int show) {
    if (!layout) return;
    
    layout->bar.visible = show;
    layout_invalidate(layout);
}

/* Set bar content */
void layout_set_bar_content(Layout *layout, View *content) {
    if (!layout) return;
    
    if (layout->bar.content) {
        view_remove_child(layout->root_view, layout->bar.content);
    }
    
    layout->bar.content = content;
    
    if (content && layout->bar.visible && layout->bar.position >= 0) {
        /* Bar is 10 pixels wide, full height */
        /* Position needs to account for regions */
        view_add_child(layout->root_view, content);
    }
    
    layout_invalidate(layout);
}

/* Get bar column position */
int layout_get_bar_column(Layout *layout) {
    if (!layout) return -1;
    return layout->bar.position;
}

/* Set active region */
void layout_set_active_region(Layout *layout, Region *region) {
    if (!layout || !region) return;
    
    /* Deactivate previous */
    if (layout->active_region) {
        layout->active_region->active = 0;
    }
    
    /* Activate new */
    layout->active_region = region;
    region->active = 1;
    
    /* Update focus if region has content */
    if (region->content) {
        layout->focus_view = region->content;
    }
    
    layout_invalidate(layout);
}

/* Focus a specific view */
void layout_focus_view(Layout *layout, View *view) {
    Region *region = NULL;
    int row, col;
    
    if (!layout) return;
    
    layout->focus_view = view;
    
    /* Find which region contains this view */
    if (view) {
        for (row = 0; row < 6; row++) {
            for (col = 0; col < 7; col++) {
                if (layout->regions[row][col].content == view) {
                    region = &layout->regions[row][col];
                    break;
                }
            }
            if (region) break;
        }
        
        if (region) {
            layout_set_active_region(layout, region);
        }
    }
}

/* Move focus in a direction */
void layout_move_focus(Layout *layout, int direction) {
    Region *current, *next = NULL;
    int x, y;
    
    if (!layout || !layout->active_region) return;
    
    current = layout->active_region;
    x = current->x;
    y = current->y;
    
    /* Find next region in direction */
    switch (direction) {
        case 0:  /* Up */
            if (y > 0) next = layout_get_region(layout, x, y - 1);
            break;
        case 1:  /* Right */
            if (x < 6) next = layout_get_region(layout, x + 1, y);
            break;
        case 2:  /* Down */
            if (y < 5) next = layout_get_region(layout, x, y + 1);
            break;
        case 3:  /* Left */
            if (x > 0) next = layout_get_region(layout, x - 1, y);
            break;
    }
    
    /* Set active if found and has content */
    if (next && next->content) {
        layout_set_active_region(layout, next);
    }
}

/* Get active region */
Region* layout_get_active_region(Layout *layout) {
    if (!layout) return NULL;
    return layout->active_region;
}

/* Get focused view */
View* layout_get_focus_view(Layout *layout) {
    if (!layout) return NULL;
    return layout->focus_view;
}

/* Draw the layout */
void layout_draw(Layout *layout, GraphicsContext *gc) {
    if (!layout || !gc) return;
    
    /* Clear background */
    gc_fill_rect(gc, 0, 0, 640, 480, layout->background_color);
    
    /* Draw regions and their content */
    layout_draw_regions(layout, gc);
    
    /* Draw bar if visible */
    if (layout->bar.visible && layout->bar.position >= 0) {
        layout_draw_bar(layout, gc);
    }
    
    layout->needs_redraw = 0;
}

/* Draw all regions */
void layout_draw_regions(Layout *layout, GraphicsContext *gc) {
    int row, col;
    Region *region;
    int x, y, w, h;
    
    if (!layout || !gc) return;
    
    /* Draw root view tree (includes all content) */
    if (layout->root_view) {
        view_draw_tree(layout->root_view, gc);
    }
    
    /* Draw region borders for active region */
    if (layout->active_region && layout->active_region->active) {
        region = layout->active_region;
        layout_region_to_pixels(region, &x, &y, &w, &h);
        
        /* Draw highlighted border */
        gc_draw_rect(gc, x, y, w - 1, h - 1, 11);  /* Gold border for active */
    }
}

/* Draw the vertical bar */
void layout_draw_bar(Layout *layout, GraphicsContext *gc) {
    int bar_x, bar_y;
    
    if (!layout || !gc || layout->bar.position < 0) return;
    
    /* Calculate bar position in pixels */
    grid_region_to_pixel(layout->bar.position, 0, &bar_x, &bar_y);
    
    /* Adjust for actual bar position (between regions) */
    if (layout->bar.position > 0) {
        bar_x -= BAR_WIDTH / 2;
    }
    
    /* Draw bar */
    gc_fill_rect(gc, bar_x, 0, BAR_WIDTH, 480, layout->bar.color);
    
    /* Draw bar content if any */
    if (layout->bar.content) {
        /* Bar content draws itself via view system */
    }
}

/* Mark layout for redraw */
void layout_invalidate(Layout *layout) {
    if (!layout) return;
    
    layout->needs_redraw = 1;
    
    /* Also invalidate root view */
    if (layout->root_view) {
        view_invalidate(layout->root_view);
    }
}

/* Handle input event */
int layout_handle_event(Layout *layout, InputEvent *event) {
    View *target_view = NULL;
    View *old_hover_view;
    Region *region;
    int handled = 0;
    
    if (!layout || !event) return 0;
    
    /* First, try dispatching through event bus if available */
    if (layout->event_bus) {
        handled = event_bus_dispatch(layout->event_bus, event);
        if (handled) {
            return 1;  /* Event was handled by bus subscriber */
        }
    }
    
    /* For mouse events, find target view */
    if (event->type == EVENT_MOUSE_DOWN || 
        event->type == EVENT_MOUSE_UP || 
        event->type == EVENT_MOUSE_MOVE) {
        
        /* Use view hit testing to find the deepest view at this position */
        /* This will properly handle child views */
        if (layout->root_view) {
            /* Use pixel-based hit test directly */
            target_view = view_hit_test_pixels(layout->root_view, 
                                              event->data.mouse.x, 
                                              event->data.mouse.y);
            
            /* Handle mouse enter/leave for hover states */
            if (event->type == EVENT_MOUSE_MOVE) {
                old_hover_view = layout->hover_view;
                
                /* Check if we've entered a new view */
                if (target_view != old_hover_view) {
                    /* Send mouse leave to old view if it had hover */
                    if (old_hover_view && old_hover_view->handle_event) {
                        InputEvent leave_event;
                        leave_event.type = EVENT_MOUSE_LEAVE;
                        leave_event.data.mouse.x = event->data.mouse.x;
                        leave_event.data.mouse.y = event->data.mouse.y;
                        leave_event.data.mouse.button = event->data.mouse.button;
                        view_handle_event(old_hover_view, &leave_event);
                    }
                    
                    /* Update hover view */
                    layout->hover_view = target_view;
                    
                    /* Send mouse enter to new view */
                    if (target_view && target_view->handle_event) {
                        InputEvent enter_event;
                        enter_event.type = EVENT_MOUSE_ENTER;
                        enter_event.data.mouse.x = event->data.mouse.x;
                        enter_event.data.mouse.y = event->data.mouse.y;
                        enter_event.data.mouse.button = event->data.mouse.button;
                        view_handle_event(target_view, &enter_event);
                    }
                }
            }
            
            /* If we found a view and it's a click, update active region and focus */
            if (target_view && event->type == EVENT_MOUSE_DOWN) {
                int reg_x, reg_y;
                
                /* Set focus to the clicked view */
                layout->focus_view = target_view;
                
                /* Find which region contains this view for active highlight */
                layout_pixels_to_region(layout, event->data.mouse.x, event->data.mouse.y, &reg_x, &reg_y);
                region = layout_get_region(layout, reg_x, reg_y);
                if (region) {
                    layout_set_active_region(layout, region);
                }
            }
        }
    } else {
        /* For keyboard events, send to focused view */
        target_view = layout->focus_view;
    }
    
    /* Send event to target view */
    if (target_view) {
        handled = view_handle_event(target_view, event);
    }
    
    /* If not handled, try global handlers */
    if (!handled) {
        /* Could handle layout-specific shortcuts here */
    }
    
    return handled;
}

/* Hit test to find region at pixel coordinates */
Region* layout_hit_test_region(Layout *layout, int pixel_x, int pixel_y) {
    int reg_x, reg_y;
    
    if (!layout) return NULL;
    
    layout_pixels_to_region(layout, pixel_x, pixel_y, &reg_x, &reg_y);
    return layout_get_region(layout, reg_x, reg_y);
}

/* Convert region to pixel coordinates */
void layout_region_to_pixels(Region *region, int *x, int *y, int *w, int *h) {
    if (!region || !x || !y || !w || !h) return;
    
    grid_region_to_pixel(region->x, region->y, x, y);
    *w = region->width * REGION_WIDTH;
    *h = region->height * REGION_HEIGHT;
    
    /* Adjust for bar if present */
    /* TODO: Account for bar position */
}

/* Convert pixel coordinates to region coordinates */
void layout_pixels_to_region(Layout *layout, int pixel_x, int pixel_y, int *reg_x, int *reg_y) {
    if (!layout || !reg_x || !reg_y) return;
    
    /* Simple division for now */
    *reg_x = pixel_x / REGION_WIDTH;
    *reg_y = pixel_y / REGION_HEIGHT;
    
    /* Clamp to valid range */
    if (*reg_x < 0) *reg_x = 0;
    if (*reg_x > 6) *reg_x = 6;
    if (*reg_y < 0) *reg_y = 0;
    if (*reg_y > 5) *reg_y = 5;
    
    /* TODO: Account for bar position */
}

/* Check if region is visible */
int layout_is_region_visible(Layout *layout, Region *region) {
    if (!layout || !region) return 0;
    
    /* For now, all regions are visible */
    /* TODO: Could check if covered by other regions */
    return 1;
}