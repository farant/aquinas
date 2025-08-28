# View System Architecture Improvements

## Current State
The current view system in Aquinas uses a simple hierarchy with function pointers for draw, update, and event handling. While functional, it has some limitations:
- Event routing is implicit through parent-child relationships
- No formal initialization contract for views
- UI structure is hardcoded in C
- Inconsistent initialization across different view types

## Proposed Improvements

### 1. Formal View Interface Pattern

#### Rationale
Currently, views have optional function pointers but no formal contract. This leads to:
- Inconsistent initialization (e.g., TextArea focus issues)
- Missing lifecycle management
- No guaranteed cleanup

#### Proposed Design
```c
typedef struct ViewInterface {
    /* Lifecycle methods - required */
    void (*init)(View *view, ViewContext *context);
    void (*destroy)(View *view);
    
    /* Parent-child relationship callbacks */
    void (*on_add_to_parent)(View *view, View *parent);
    void (*on_remove_from_parent)(View *view, View *parent);
    void (*on_child_added)(View *view, View *child);
    void (*on_child_removed)(View *view, View *child);
    
    /* State changes */
    void (*on_focus_gained)(View *view);
    void (*on_focus_lost)(View *view);
    void (*on_visibility_changed)(View *view, int visible);
    
    /* Required methods */
    int (*can_focus)(View *view);
    RegionRect (*get_preferred_size)(View *view);
} ViewInterface;

typedef struct ViewContext {
    Layout *layout;
    EventBus *event_bus;
    ResourceManager *resources;
    ThemeManager *theme;
} ViewContext;
```

#### Benefits
- Guaranteed proper initialization
- Consistent behavior across all views
- Better resource management
- Clear extension points

#### Implementation Steps
1. Create ViewInterface structure
2. Add interface pointer to base View struct
3. Update view_create to require interface
4. Migrate existing views one at a time
5. Add validation/debug checks

### 2. Event Bus Architecture

#### Rationale
Current event flow is rigid (layout → view hierarchy). An event bus would provide:
- Decoupled event handling
- Global event subscriptions
- Better debugging capabilities
- Foundation for complex interactions (drag & drop, tooltips, etc.)

#### Proposed Design
```c
typedef enum {
    EVENT_PRIORITY_SYSTEM = 0,    /* Highest priority */
    EVENT_PRIORITY_CAPTURE = 1,   /* Modal dialogs, etc */
    EVENT_PRIORITY_NORMAL = 2,    /* Regular views */
    EVENT_PRIORITY_BUBBLE = 3,    /* Parent handlers */
    EVENT_PRIORITY_DEFAULT = 4    /* Fallback handlers */
} EventPriority;

typedef struct EventSubscription {
    View *subscriber;
    EventType event_mask;
    EventPriority priority;
    int (*handler)(View *view, Event *event, void *context);
    void *context;
    struct EventSubscription *next;
} EventSubscription;

typedef struct EventBus {
    EventSubscription *subscriptions[EVENT_TYPE_COUNT];
    int capture_count;
    View *capture_view;  /* For modal behavior */
    
    /* Debug/metrics */
    unsigned long events_dispatched;
    unsigned long events_handled;
} EventBus;

/* API */
int event_bus_subscribe(EventBus *bus, View *view, EventType type, 
                        EventPriority priority, EventHandler handler);
void event_bus_unsubscribe(EventBus *bus, View *view, EventType type);
int event_bus_dispatch(EventBus *bus, Event *event);
void event_bus_capture(EventBus *bus, View *view);  /* Modal */
void event_bus_release_capture(EventBus *bus);
```

#### Event Flow
1. Event enters bus via `event_bus_dispatch`
2. Check for capture (modal views)
3. Process by priority:
   - SYSTEM: OS-level shortcuts
   - CAPTURE: Modal dialogs
   - NORMAL: Focused view, then parents
   - BUBBLE: Parent chain
   - DEFAULT: Global handlers

#### Benefits
- Views can subscribe to any events (not just their own)
- Modal dialogs become trivial
- Global shortcuts easy to implement
- Event flow is debuggable/traceable
- Enables advanced features (event recording/playback)

### 3. Data-Driven UI with Simple Markup

#### Rationale
Hardcoding UI in C has limitations:
- Requires recompilation for UI changes
- Mixing layout and logic
- Hard to visualize structure
- No design/developer separation

#### Proposed Minimal XML Format
```xml
<!-- Simple tags with attributes only -->
<ui>
    <panel id="main" x="0" y="0" width="640" height="480" bg="THEME_BG">
        <label id="title" text="Aquinas OS" x="10" y="10" font="9x16"/>
        
        <panel id="buttons" x="10" y="40" width="200" height="100">
            <button id="btn_ok" text="OK" x="0" y="0" onclick="handle_ok"/>
            <button id="btn_cancel" text="Cancel" x="0" y="30" onclick="handle_cancel"/>
        </panel>
        
        <textarea id="editor" x="10" y="150" width="300" height="200">
            Default text content here
        </textarea>
    </panel>
</ui>
```

#### Parser Requirements
- Minimal: Just tags, attributes, and text content
- No namespaces, no DTD, no complex features
- Single-pass parsing
- Fixed buffer sizes (no dynamic allocation during parse)
- Clear error messages

#### Implementation Approach
```c
typedef struct XmlNode {
    char tag[32];
    char id[32];
    XmlAttribute attributes[MAX_ATTRIBUTES];
    char *text_content;
    struct XmlNode *children;
    struct XmlNode *next_sibling;
} XmlNode;

typedef struct XmlAttribute {
    char name[32];
    char value[64];
} XmlAttribute;

/* Parse XML into tree */
XmlNode* xml_parse(const char *xml_string);

/* Convert XML tree to View tree */
View* ui_from_xml(XmlNode *root, ViewContext *context);

/* Find view by ID */
View* ui_find_by_id(View *root, const char *id);

/* Connect event handlers */
void ui_connect_handler(View *view, const char *event, EventHandler handler);
```

#### Benefits
- UI designers can work without touching C code
- Rapid prototyping
- Could add hot-reload in development
- Foundation for themes/styling
- Enables UI builder tools later

#### Progressive Enhancement
1. **Phase 1**: Basic element creation from XML
2. **Phase 2**: Event handler binding
3. **Phase 3**: Data binding (`text="{model.title}"`)
4. **Phase 4**: Conditionals/loops (`if="condition"`, `foreach="items"`)
5. **Phase 5**: External style sheets

## Implementation Priority

### Phase 1: View Interface (1-2 days)
- Define ViewInterface structure
- Add to View base
- Update view_create
- Migrate TextInput as proof of concept
- Add debug validation

### Phase 2: Event Bus (2-3 days)
- Implement basic pub/sub
- Add to Layout
- Route existing events through bus
- Add debug/trace capabilities
- Test with TextArea focus issue

### Phase 3: XML UI (3-5 days)
- Implement minimal XML parser
- Create ui_from_xml converter
- Convert demo UI to XML
- Add error handling
- Documentation

## Backwards Compatibility

All improvements should be backwards compatible:
- Old views work without ViewInterface (with warnings)
- Event bus runs alongside current system initially
- XML is optional - C API remains

## Testing Strategy

Each improvement needs tests:
1. **View Interface**: Lifecycle test suite
2. **Event Bus**: Event routing tests, priority tests
3. **XML**: Parser tests, malformed input tests

## Performance Considerations

- Event bus: Use hash table for O(1) lookup
- XML parsing: One-time cost at UI creation
- View interface: Minimal overhead (one extra pointer)

## Debug/Development Tools

These improvements enable better tooling:
- Event inspector (show event flow)
- View hierarchy visualizer
- Live XML reload
- Performance profiler hooks

## Long-term Vision

These improvements lay groundwork for:
- Component library (reusable widgets)
- Theme system
- Accessibility features
- UI animation system
- Layout managers (flex, grid)
- Data binding framework

## Migration Path

1. Implement alongside existing system
2. Migrate internal components
3. Provide migration guide for users
4. Deprecate old APIs (with long support window)
5. Remove old code in major version

## Open Questions

- Should event bus be global or per-layout?
- XML format: attributes only or support nested text?
- How to handle custom components in XML?
- Should view interface be vtable or inline functions?
- Memory allocation strategy for XML nodes?

## References

- Win32 message system (for event bus ideas)
- GTK signal system (for event subscription)
- Android layout XML (for markup inspiration)
- React component lifecycle (for view interface)