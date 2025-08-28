#ifndef EVENT_BUS_H
#define EVENT_BUS_H

#include "view.h"
#include "input.h"

/* Event priority levels for ordered processing */
typedef enum {
    EVENT_PRIORITY_SYSTEM = 0,    /* Highest priority - OS shortcuts */
    EVENT_PRIORITY_CAPTURE = 1,   /* Modal dialogs and captured input */
    EVENT_PRIORITY_NORMAL = 2,    /* Regular view handlers */
    EVENT_PRIORITY_BUBBLE = 3,    /* Parent chain handlers */
    EVENT_PRIORITY_DEFAULT = 4    /* Fallback handlers */
} EventPriority;

/* Forward declarations */
typedef struct EventBus EventBus;
typedef struct EventSubscription EventSubscription;

/* Event handler callback type
 * Returns 1 if event was handled (stop propagation), 0 otherwise */
typedef int (*EventBusHandler)(View *view, InputEvent *event, void *context);

/* Event subscription node */
struct EventSubscription {
    View *subscriber;              /* The subscribing view */
    EventType event_mask;         /* Types of events to receive */
    EventPriority priority;       /* Processing priority */
    EventBusHandler handler;     /* Callback function */
    void *context;               /* User data for handler */
    struct EventSubscription *next;  /* Next in priority list */
};

/* Main event bus structure */
struct EventBus {
    /* Subscription lists by event type */
    EventSubscription *subscriptions[EVENT_TYPE_COUNT];
    
    /* Modal capture state */
    View *capture_view;          /* View with exclusive input */
    int capture_count;           /* Nested capture count */
    
    /* Debug/metrics */
    unsigned long events_dispatched;
    unsigned long events_handled;
    
    /* Memory pool for subscriptions (avoid malloc) */
    EventSubscription subscription_pool[256];
    EventSubscription *free_list;
    int pool_initialized;
};

/* API Functions */

/* Create and destroy event bus */
EventBus* event_bus_create(void);
void event_bus_destroy(EventBus *bus);

/* Subscribe/unsubscribe to events */
int event_bus_subscribe(EventBus *bus, View *view, EventType type, 
                       EventPriority priority, EventBusHandler handler, void *context);
void event_bus_unsubscribe(EventBus *bus, View *view, EventType type);
void event_bus_unsubscribe_all(EventBus *bus, View *view);

/* Dispatch events through the bus */
int event_bus_dispatch(EventBus *bus, InputEvent *event);

/* Modal capture (exclusive input) */
void event_bus_capture(EventBus *bus, View *view);
void event_bus_release_capture(EventBus *bus);
int event_bus_has_capture(EventBus *bus, View *view);

/* Debug utilities */
void event_bus_dump_stats(EventBus *bus);
void event_bus_dump_subscriptions(EventBus *bus);

#endif /* EVENT_BUS_H */