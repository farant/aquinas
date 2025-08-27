/* View System Implementation
 * 
 * Provides a hierarchical view system for building UI components.
 * Views can be nested, have custom drawing and event handling,
 * and are positioned using the region coordinate system.
 */

#include "view.h"
#include "memory.h"
#include "serial.h"
#include "grid.h"

/* Create a new view with the specified bounds in region coordinates */
View* view_create(int x, int y, int width, int height) {
    View *view = (View*)malloc(sizeof(View));
    if (!view) {
        serial_write_string("ERROR: Failed to allocate view\n");
        return NULL;
    }
    
    /* Initialize bounds */
    view->bounds.x = x;
    view->bounds.y = y;
    view->bounds.width = width;
    view->bounds.height = height;
    
    /* Initialize hierarchy pointers */
    view->parent = NULL;
    view->children = NULL;
    view->next_sibling = NULL;
    
    /* Initialize properties */
    view->visible = 1;
    view->needs_redraw = 1;
    view->z_order = 0;
    
    /* Initialize state */
    view->user_data = NULL;
    
    /* Initialize methods to NULL - subclasses will override */
    view->draw = NULL;
    view->update = NULL;
    view->handle_event = NULL;
    view->destroy = NULL;
    
    view->type_name = "View";
    
    return view;
}

/* Destroy a view and all its children */
void view_destroy(View *view) {
    View *child, *next_child;
    
    if (!view) return;
    
    /* Call custom destroy if provided */
    if (view->destroy) {
        view->destroy(view);
    }
    
    /* Destroy all children */
    child = view->children;
    while (child) {
        next_child = child->next_sibling;
        view_destroy(child);
        child = next_child;
    }
    
    /* Remove from parent if attached */
    if (view->parent) {
        view_remove_child(view->parent, view);
    }
    
    /* Note: In a real system we'd free the memory here */
    /* Our allocator doesn't support free yet */
}

/* Add a child view to a parent */
void view_add_child(View *parent, View *child) {
    View *sibling;
    
    if (!parent || !child) return;
    
    /* Remove from previous parent if any */
    if (child->parent) {
        view_remove_child(child->parent, child);
    }
    
    /* Set new parent */
    child->parent = parent;
    
    /* Add to end of children list */
    if (!parent->children) {
        parent->children = child;
    } else {
        /* Find last child */
        sibling = parent->children;
        while (sibling->next_sibling) {
            sibling = sibling->next_sibling;
        }
        sibling->next_sibling = child;
    }
    
    child->next_sibling = NULL;
    
    /* Mark parent for redraw */
    view_invalidate(parent);
}

/* Remove a child view from its parent */
void view_remove_child(View *parent, View *child) {
    View *prev, *curr;
    
    if (!parent || !child || child->parent != parent) return;
    
    /* Find and unlink the child */
    if (parent->children == child) {
        parent->children = child->next_sibling;
    } else {
        prev = parent->children;
        curr = prev->next_sibling;
        while (curr) {
            if (curr == child) {
                prev->next_sibling = curr->next_sibling;
                break;
            }
            prev = curr;
            curr = curr->next_sibling;
        }
    }
    
    child->parent = NULL;
    child->next_sibling = NULL;
    
    /* Mark parent for redraw */
    view_invalidate(parent);
}

/* Get the root view of the hierarchy */
View* view_get_root(View *view) {
    if (!view) return NULL;
    
    while (view->parent) {
        view = view->parent;
    }
    
    return view;
}

/* Set view visibility */
void view_set_visible(View *view, int visible) {
    if (!view || view->visible == visible) return;
    
    view->visible = visible;
    view_invalidate(view);
    
    /* If hiding, also hide focus/hover states */
    if (!visible) {
        /* TODO: Handle focus loss */
    }
}

/* Set view bounds */
void view_set_bounds(View *view, int x, int y, int width, int height) {
    if (!view) return;
    
    /* Invalidate old area */
    view_invalidate(view);
    
    /* Update bounds */
    view->bounds.x = x;
    view->bounds.y = y;
    view->bounds.width = width;
    view->bounds.height = height;
    
    /* Invalidate new area */
    view_invalidate(view);
}

/* Bring view to front among siblings */
void view_bring_to_front(View *view) {
    View *parent, *prev, *curr;
    
    if (!view || !view->parent) return;
    
    parent = view->parent;
    
    /* Already at front? */
    if (!view->next_sibling) return;
    
    /* Remove from current position */
    if (parent->children == view) {
        parent->children = view->next_sibling;
    } else {
        prev = parent->children;
        curr = prev->next_sibling;
        while (curr) {
            if (curr == view) {
                prev->next_sibling = curr->next_sibling;
                break;
            }
            prev = curr;
            curr = curr->next_sibling;
        }
    }
    
    /* Add to end */
    if (!parent->children) {
        parent->children = view;
    } else {
        curr = parent->children;
        while (curr->next_sibling) {
            curr = curr->next_sibling;
        }
        curr->next_sibling = view;
    }
    view->next_sibling = NULL;
    
    view_invalidate(parent);
}

/* Send view to back among siblings */
void view_send_to_back(View *view) {
    View *parent, *prev, *curr;
    
    if (!view || !view->parent) return;
    
    parent = view->parent;
    
    /* Already at back? */
    if (parent->children == view) return;
    
    /* Remove from current position */
    prev = parent->children;
    curr = prev->next_sibling;
    while (curr) {
        if (curr == view) {
            prev->next_sibling = curr->next_sibling;
            break;
        }
        prev = curr;
        curr = curr->next_sibling;
    }
    
    /* Add to front */
    view->next_sibling = parent->children;
    parent->children = view;
    
    view_invalidate(parent);
}

/* Mark view for redraw */
void view_invalidate(View *view) {
    if (!view) return;
    
    view->needs_redraw = 1;
    
    /* Propagate up to parent */
    if (view->parent) {
        view_invalidate(view->parent);
    }
}

/* Mark specific rectangle for redraw */
void view_invalidate_rect(View *view, RegionRect *rect) {
    /* For now, just invalidate the whole view */
    /* TODO: Implement dirty rectangle tracking */
    view_invalidate(view);
}

/* Draw a view and all its children */
void view_draw_tree(View *root, GraphicsContext *gc) {
    View *child;
    RegionRect abs_bounds;
    int pixel_x, pixel_y;
    
    if (!root || !gc) return;
    
    /* Skip invisible views */
    if (!root->visible) return;
    
    /* Get absolute bounds */
    view_get_absolute_bounds(root, &abs_bounds);
    
    /* Convert to pixel coordinates */
    grid_region_to_pixel(abs_bounds.x, abs_bounds.y, &pixel_x, &pixel_y);
    
    /* Save graphics context state */
    /* TODO: Save/restore clip and translation */
    
    /* Set up clipping for this view */
    gc_set_clip(gc, pixel_x, pixel_y, 
                abs_bounds.width * REGION_WIDTH, 
                abs_bounds.height * REGION_HEIGHT);
    
    /* Draw the view itself */
    if (root->draw) {
        root->draw(root, gc);
    }
    
    /* Draw children in order */
    child = root->children;
    while (child) {
        view_draw_tree(child, gc);
        child = child->next_sibling;
    }
    
    /* Clear needs_redraw flag */
    root->needs_redraw = 0;
    
    /* Restore graphics context state */
    /* TODO: Restore clip and translation */
}

/* Draw a single view (without children) */
void view_draw(View *view, GraphicsContext *gc) {
    if (!view || !gc || !view->visible) return;
    
    if (view->draw) {
        view->draw(view, gc);
    }
    
    view->needs_redraw = 0;
}

/* Update view tree with time delta */
void view_update_tree(View *root, int delta_ms) {
    View *child;
    
    if (!root || !root->visible) return;
    
    /* Update this view */
    if (root->update) {
        root->update(root, delta_ms);
    }
    
    /* Update children */
    child = root->children;
    while (child) {
        view_update_tree(child, delta_ms);
        child = child->next_sibling;
    }
}

/* Find view at screen coordinates */
View* view_hit_test(View *root, int x, int y) {
    View *child, *hit;
    RegionRect abs_bounds;
    
    if (!root || !root->visible) return NULL;
    
    /* Check if point is within this view */
    view_get_absolute_bounds(root, &abs_bounds);
    if (x < abs_bounds.x || x >= abs_bounds.x + abs_bounds.width ||
        y < abs_bounds.y || y >= abs_bounds.y + abs_bounds.height) {
        return NULL;
    }
    
    /* Check children in reverse order (front to back) */
    /* First, build a reverse list */
    /* TODO: This is inefficient, could maintain a reverse pointer */
    child = root->children;
    while (child) {
        hit = view_hit_test(child, x, y);
        if (hit) return hit;
        child = child->next_sibling;
    }
    
    /* If no child hit, return this view */
    return root;
}

/* Handle event for view and potentially its children */
int view_handle_event(View *view, InputEvent *event) {
    if (!view || !event) return 0;
    
    if (view->handle_event) {
        return view->handle_event(view, event);
    }
    
    return 0;  /* Event not handled */
}

/* Broadcast event to all views in tree */
void view_broadcast_event(View *root, InputEvent *event) {
    View *child;
    
    if (!root || !event || !root->visible) return;
    
    /* Send to this view */
    if (root->handle_event) {
        if (root->handle_event(root, event)) {
            return;  /* Event was handled, stop propagation */
        }
    }
    
    /* Send to children */
    child = root->children;
    while (child) {
        view_broadcast_event(child, event);
        child = child->next_sibling;
    }
}

/* Convert screen coordinates to view-local coordinates */
void view_screen_to_local(View *view, int screen_x, int screen_y, int *local_x, int *local_y) {
    RegionRect abs_bounds;
    
    if (!view || !local_x || !local_y) return;
    
    view_get_absolute_bounds(view, &abs_bounds);
    
    *local_x = screen_x - abs_bounds.x;
    *local_y = screen_y - abs_bounds.y;
}

/* Convert view-local coordinates to screen coordinates */
void view_local_to_screen(View *view, int local_x, int local_y, int *screen_x, int *screen_y) {
    RegionRect abs_bounds;
    
    if (!view || !screen_x || !screen_y) return;
    
    view_get_absolute_bounds(view, &abs_bounds);
    
    *screen_x = local_x + abs_bounds.x;
    *screen_y = local_y + abs_bounds.y;
}

/* Check if point is within view bounds */
int view_contains_point(View *view, int x, int y) {
    RegionRect abs_bounds;
    
    if (!view) return 0;
    
    view_get_absolute_bounds(view, &abs_bounds);
    
    return (x >= abs_bounds.x && x < abs_bounds.x + abs_bounds.width &&
            y >= abs_bounds.y && y < abs_bounds.y + abs_bounds.height);
}

/* Get absolute screen bounds of view */
void view_get_absolute_bounds(View *view, RegionRect *abs_bounds) {
    int offset_x = 0, offset_y = 0;
    View *parent;
    
    if (!view || !abs_bounds) return;
    
    /* Start with view's own bounds */
    *abs_bounds = view->bounds;
    
    /* Walk up the parent chain, accumulating offsets */
    parent = view->parent;
    while (parent) {
        offset_x += parent->bounds.x;
        offset_y += parent->bounds.y;
        parent = parent->parent;
    }
    
    abs_bounds->x += offset_x;
    abs_bounds->y += offset_y;
}