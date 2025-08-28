#ifndef VIEW_INTERFACE_H
#define VIEW_INTERFACE_H

#include "view.h"

/* Forward declarations */
#ifndef LAYOUT_TYPEDEF
#define LAYOUT_TYPEDEF
typedef struct Layout Layout;
#endif

#ifndef EVENTBUS_TYPEDEF
#define EVENTBUS_TYPEDEF
typedef struct EventBus EventBus;
#endif

typedef struct ResourceManager ResourceManager;
typedef struct ThemeManager ThemeManager;

/* View context passed during initialization */
typedef struct ViewContext {
    Layout *layout;
    EventBus *event_bus;         /* Will be NULL initially */
    ResourceManager *resources;  /* Will be NULL initially */
    ThemeManager *theme;         /* Will be NULL initially */
} ViewContext;

/* View lifecycle interface */
typedef struct ViewInterface {
    /* Lifecycle methods - required */
    void (*init)(View *view, ViewContext *context);
    void (*destroy)(View *view);
    
    /* Parent-child relationship callbacks - optional */
    void (*on_add_to_parent)(View *view, View *parent);
    void (*on_remove_from_parent)(View *view, View *parent);
    void (*on_child_added)(View *view, View *child);
    void (*on_child_removed)(View *view, View *child);
    
    /* State changes - optional */
    void (*on_focus_gained)(View *view);
    void (*on_focus_lost)(View *view);
    void (*on_visibility_changed)(View *view, int visible);
    void (*on_enabled_changed)(View *view, int enabled);
    
    /* Required capability methods */
    int (*can_focus)(View *view);
    RegionRect (*get_preferred_size)(View *view);
    
    /* Drawing and events - uses existing View function pointers */
    /* These are kept in View for backwards compatibility */
} ViewInterface;

/* Helper functions for view interface management */
void view_interface_init(View *view, ViewInterface *interface, ViewContext *context);
void view_interface_destroy(View *view);
void view_interface_notify_focus_gained(View *view);
void view_interface_notify_focus_lost(View *view);
void view_interface_notify_parent_changed(View *view, View *old_parent, View *new_parent);
void view_interface_notify_child_changed(View *view, View *child, int added);
void view_interface_notify_visibility_changed(View *view, int visible);
void view_interface_notify_enabled_changed(View *view, int enabled);

/* Default implementations for optional methods */
void view_interface_default_on_add_to_parent(View *view, View *parent);
void view_interface_default_on_remove_from_parent(View *view, View *parent);
void view_interface_default_on_child_added(View *view, View *child);
void view_interface_default_on_child_removed(View *view, View *child);
void view_interface_default_on_focus_gained(View *view);
void view_interface_default_on_focus_lost(View *view);
void view_interface_default_on_visibility_changed(View *view, int visible);
void view_interface_default_on_enabled_changed(View *view, int enabled);

#endif /* VIEW_INTERFACE_H */