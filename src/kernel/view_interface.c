#include "view_interface.h"
#include "serial.h"

#define NULL ((void*)0)

/* Initialize a view with its interface */
void view_interface_init(View *view, ViewInterface *interface, ViewContext *context) {
    if (!view || !interface) {
        return;
    }
    
    view->interface = interface;
    
    /* Call the init method if provided */
    if (interface->init) {
        interface->init(view, context);
    }
    
    serial_write_string("ViewInterface: Initialized view with interface\n");
}

/* Destroy a view through its interface */
void view_interface_destroy(View *view) {
    if (!view || !view->interface) {
        return;
    }
    
    /* Call the destroy method if provided */
    if (view->interface->destroy) {
        view->interface->destroy(view);
    }
    
    view->interface = NULL;
}

/* Notify focus gained */
void view_interface_notify_focus_gained(View *view) {
    if (!view || !view->interface) {
        return;
    }
    
    if (view->interface->on_focus_gained) {
        view->interface->on_focus_gained(view);
    } else {
        view_interface_default_on_focus_gained(view);
    }
}

/* Notify focus lost */
void view_interface_notify_focus_lost(View *view) {
    if (!view || !view->interface) {
        return;
    }
    
    if (view->interface->on_focus_lost) {
        view->interface->on_focus_lost(view);
    } else {
        view_interface_default_on_focus_lost(view);
    }
}

/* Notify parent changed */
void view_interface_notify_parent_changed(View *view, View *old_parent, View *new_parent) {
    if (!view || !view->interface) {
        return;
    }
    
    /* Notify removal from old parent */
    if (old_parent) {
        if (view->interface->on_remove_from_parent) {
            view->interface->on_remove_from_parent(view, old_parent);
        } else {
            view_interface_default_on_remove_from_parent(view, old_parent);
        }
        
        /* Notify old parent of child removal */
        if (old_parent->interface && old_parent->interface->on_child_removed) {
            old_parent->interface->on_child_removed(old_parent, view);
        }
    }
    
    /* Notify addition to new parent */
    if (new_parent) {
        if (view->interface->on_add_to_parent) {
            view->interface->on_add_to_parent(view, new_parent);
        } else {
            view_interface_default_on_add_to_parent(view, new_parent);
        }
        
        /* Notify new parent of child addition */
        if (new_parent->interface && new_parent->interface->on_child_added) {
            new_parent->interface->on_child_added(new_parent, view);
        }
    }
}

/* Notify child changed */
void view_interface_notify_child_changed(View *view, View *child, int added) {
    if (!view || !view->interface || !child) {
        return;
    }
    
    if (added) {
        if (view->interface->on_child_added) {
            view->interface->on_child_added(view, child);
        } else {
            view_interface_default_on_child_added(view, child);
        }
    } else {
        if (view->interface->on_child_removed) {
            view->interface->on_child_removed(view, child);
        } else {
            view_interface_default_on_child_removed(view, child);
        }
    }
}

/* Notify visibility changed */
void view_interface_notify_visibility_changed(View *view, int visible) {
    if (!view || !view->interface) {
        return;
    }
    
    if (view->interface->on_visibility_changed) {
        view->interface->on_visibility_changed(view, visible);
    } else {
        view_interface_default_on_visibility_changed(view, visible);
    }
}

/* Notify enabled state changed */
void view_interface_notify_enabled_changed(View *view, int enabled) {
    if (!view || !view->interface) {
        return;
    }
    
    if (view->interface->on_enabled_changed) {
        view->interface->on_enabled_changed(view, enabled);
    } else {
        view_interface_default_on_enabled_changed(view, enabled);
    }
}

/* Default implementations */

void view_interface_default_on_add_to_parent(View *view, View *parent) {
    /* Default: no action */
    (void)view;
    (void)parent;
}

void view_interface_default_on_remove_from_parent(View *view, View *parent) {
    /* Default: no action */
    (void)view;
    (void)parent;
}

void view_interface_default_on_child_added(View *view, View *child) {
    /* Default: no action */
    (void)view;
    (void)child;
}

void view_interface_default_on_child_removed(View *view, View *child) {
    /* Default: no action */
    (void)view;
    (void)child;
}

void view_interface_default_on_focus_gained(View *view) {
    /* Default: mark for redraw */
    if (view) {
        view->needs_redraw = 1;
        serial_write_string("ViewInterface: Default focus gained - marking for redraw\n");
    }
}

void view_interface_default_on_focus_lost(View *view) {
    /* Default: mark for redraw */
    if (view) {
        view->needs_redraw = 1;
        serial_write_string("ViewInterface: Default focus lost - marking for redraw\n");
    }
}

void view_interface_default_on_visibility_changed(View *view, int visible) {
    /* Default: mark for redraw */
    if (view) {
        view->needs_redraw = 1;
        if (visible) {
            serial_write_string("ViewInterface: Default visibility changed - now visible\n");
        } else {
            serial_write_string("ViewInterface: Default visibility changed - now hidden\n");
        }
    }
}

void view_interface_default_on_enabled_changed(View *view, int enabled) {
    /* Default: mark for redraw */
    if (view) {
        view->needs_redraw = 1;
        if (enabled) {
            serial_write_string("ViewInterface: Default enabled changed - now enabled\n");
        } else {
            serial_write_string("ViewInterface: Default enabled changed - now disabled\n");
        }
    }
}