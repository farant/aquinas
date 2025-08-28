/* Event Bus Implementation 
 * 
 * Provides decoupled event routing with priority-based dispatch
 */

#include "event_bus.h"
#include "memory.h"
#include "serial.h"

#define NULL ((void*)0)

/* Initialize the subscription memory pool */
static void init_subscription_pool(EventBus *bus) {
    int i;
    
    if (bus->pool_initialized) return;
    
    /* Link all subscriptions into free list */
    for (i = 0; i < 255; i++) {
        bus->subscription_pool[i].next = &bus->subscription_pool[i + 1];
    }
    bus->subscription_pool[255].next = NULL;
    bus->free_list = &bus->subscription_pool[0];
    bus->pool_initialized = 1;
}

/* Allocate subscription from pool */
static EventSubscription* alloc_subscription(EventBus *bus) {
    EventSubscription *sub;
    
    if (!bus->pool_initialized) {
        init_subscription_pool(bus);
    }
    
    if (!bus->free_list) {
        serial_write_string("ERROR: Event bus subscription pool exhausted\n");
        return NULL;
    }
    
    sub = bus->free_list;
    bus->free_list = sub->next;
    
    /* Clear the subscription */
    memset(sub, 0, sizeof(EventSubscription));
    
    return sub;
}

/* Free subscription back to pool */
static void free_subscription(EventBus *bus, EventSubscription *sub) {
    if (!sub) return;
    
    sub->next = bus->free_list;
    bus->free_list = sub;
}

/* Create event bus */
EventBus* event_bus_create(void) {
    EventBus *bus;
    int i;
    
    /* Allocate bus structure */
    bus = (EventBus*)malloc(sizeof(EventBus));
    if (!bus) {
        serial_write_string("ERROR: Failed to allocate event bus\n");
        return NULL;
    }
    
    /* Initialize */
    memset(bus, 0, sizeof(EventBus));
    
    /* Clear subscription lists */
    for (i = 0; i < EVENT_TYPE_COUNT; i++) {
        bus->subscriptions[i] = NULL;
    }
    
    /* Initialize pool */
    init_subscription_pool(bus);
    
    serial_write_string("Event bus created\n");
    
    return bus;
}

/* Destroy event bus */
void event_bus_destroy(EventBus *bus) {
    int i;
    EventSubscription *sub, *next;
    
    if (!bus) return;
    
    serial_write_string("Destroying event bus\n");
    
    /* Clear all subscriptions (they're in the pool, so just reset pointers) */
    for (i = 0; i < EVENT_TYPE_COUNT; i++) {
        bus->subscriptions[i] = NULL;
    }
    
    /* Free the bus structure */
    free(bus);
}

/* Subscribe to events */
int event_bus_subscribe(EventBus *bus, View *view, EventType type, 
                       EventPriority priority, EventBusHandler handler, void *context) {
    EventSubscription *sub, *current, *prev;
    
    if (!bus || !view || !handler) return 0;
    if (type >= EVENT_TYPE_COUNT) return 0;
    
    /* Allocate new subscription */
    sub = alloc_subscription(bus);
    if (!sub) return 0;
    
    /* Fill in subscription */
    sub->subscriber = view;
    sub->event_mask = type;
    sub->priority = priority;
    sub->handler = handler;
    sub->context = context;
    sub->next = NULL;
    
    /* Insert into priority-sorted list */
    current = bus->subscriptions[type];
    prev = NULL;
    
    /* Find insertion point (lower priority value = higher priority) */
    while (current && current->priority <= priority) {
        prev = current;
        current = current->next;
    }
    
    /* Insert subscription */
    if (prev) {
        sub->next = prev->next;
        prev->next = sub;
    } else {
        sub->next = bus->subscriptions[type];
        bus->subscriptions[type] = sub;
    }
    
    serial_write_string("Event subscription added for type ");
    serial_write_hex(type);
    serial_write_string(" priority ");
    serial_write_hex(priority);
    serial_write_string("\n");
    
    return 1;
}

/* Unsubscribe from specific event type */
void event_bus_unsubscribe(EventBus *bus, View *view, EventType type) {
    EventSubscription *current, *prev, *next;
    
    if (!bus || !view || type >= EVENT_TYPE_COUNT) return;
    
    current = bus->subscriptions[type];
    prev = NULL;
    
    while (current) {
        next = current->next;
        
        if (current->subscriber == view) {
            /* Remove from list */
            if (prev) {
                prev->next = next;
            } else {
                bus->subscriptions[type] = next;
            }
            
            /* Free subscription */
            free_subscription(bus, current);
            
            serial_write_string("Event unsubscribed for type ");
            serial_write_hex(type);
            serial_write_string("\n");
            
            /* Continue searching (view might have multiple handlers) */
            current = next;
        } else {
            prev = current;
            current = next;
        }
    }
}

/* Unsubscribe from all events */
void event_bus_unsubscribe_all(EventBus *bus, View *view) {
    int i;
    
    if (!bus || !view) return;
    
    for (i = 0; i < EVENT_TYPE_COUNT; i++) {
        event_bus_unsubscribe(bus, view, i);
    }
}

/* Dispatch event through bus */
int event_bus_dispatch(EventBus *bus, InputEvent *event) {
    EventSubscription *sub;
    int handled = 0;
    
    if (!bus || !event) return 0;
    if (event->type >= EVENT_TYPE_COUNT) return 0;
    
    bus->events_dispatched++;
    
    /* Check for modal capture */
    if (bus->capture_view) {
        /* Only send to capture view's handlers */
        sub = bus->subscriptions[event->type];
        while (sub) {
            if (sub->subscriber == bus->capture_view) {
                if (sub->handler(sub->subscriber, event, sub->context)) {
                    bus->events_handled++;
                    return 1;  /* Captured and handled */
                }
            }
            sub = sub->next;
        }
        /* Captured but not handled */
        return 0;
    }
    
    /* Normal dispatch - go through priority list */
    sub = bus->subscriptions[event->type];
    while (sub && !handled) {
        /* Call handler */
        if (sub->handler(sub->subscriber, event, sub->context)) {
            handled = 1;
            bus->events_handled++;
            /* Continue for EVENT_PRIORITY_DEFAULT to allow multiple handlers */
            if (sub->priority != EVENT_PRIORITY_DEFAULT) {
                break;
            }
        }
        sub = sub->next;
    }
    
    return handled;
}

/* Set modal capture */
void event_bus_capture(EventBus *bus, View *view) {
    if (!bus || !view) return;
    
    if (bus->capture_view == view) {
        bus->capture_count++;
    } else {
        bus->capture_view = view;
        bus->capture_count = 1;
    }
    
    serial_write_string("Event bus captured by view\n");
}

/* Release modal capture */
void event_bus_release_capture(EventBus *bus) {
    if (!bus || !bus->capture_view) return;
    
    bus->capture_count--;
    if (bus->capture_count <= 0) {
        bus->capture_view = NULL;
        bus->capture_count = 0;
        serial_write_string("Event bus capture released\n");
    }
}

/* Check if view has capture */
int event_bus_has_capture(EventBus *bus, View *view) {
    return bus && bus->capture_view == view;
}

/* Debug: dump statistics */
void event_bus_dump_stats(EventBus *bus) {
    if (!bus) return;
    
    serial_write_string("Event Bus Stats:\n");
    serial_write_string("  Events dispatched: ");
    serial_write_hex(bus->events_dispatched);
    serial_write_string("\n  Events handled: ");
    serial_write_hex(bus->events_handled);
    serial_write_string("\n  Capture view: ");
    serial_write_hex((unsigned int)bus->capture_view);
    serial_write_string("\n");
}

/* Debug: dump subscriptions */
void event_bus_dump_subscriptions(EventBus *bus) {
    int i;
    EventSubscription *sub;
    int count;
    
    if (!bus) return;
    
    serial_write_string("Event Bus Subscriptions:\n");
    
    for (i = 0; i < EVENT_TYPE_COUNT; i++) {
        count = 0;
        sub = bus->subscriptions[i];
        
        while (sub) {
            count++;
            sub = sub->next;
        }
        
        if (count > 0) {
            serial_write_string("  Type ");
            serial_write_hex(i);
            serial_write_string(": ");
            serial_write_hex(count);
            serial_write_string(" handlers\n");
        }
    }
}