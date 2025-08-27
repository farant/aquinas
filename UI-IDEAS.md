Here’s the layered API architecture based on our discussion:

## Layer 1: Device Driver Interface

```c
typedef struct {
    int width, height, bpp;
    void (*init)(void);
    void (*shutdown)(void);
    void (*set_pixel)(int x, int y, unsigned char color);
    unsigned char (*get_pixel)(int x, int y);
    void (*fill_rect)(int x, int y, int w, int h, unsigned char color);
    void (*blit)(int x, int y, int w, int h, unsigned char *src, int src_stride);
    void (*set_palette)(unsigned char palette[16][3]);
} DisplayDriver;
```

## Layer 2: Graphics Primitives

```c
void gfx_draw_line(int x0, int y0, int x1, int y1, unsigned char color);
void gfx_draw_rect(int x, int y, int w, int h, unsigned char color);
void gfx_fill_rect(int x, int y, int w, int h, unsigned char color);
void gfx_draw_circle(int cx, int cy, int r, unsigned char color);
void gfx_fill_circle(int cx, int cy, int r, unsigned char color);
void gfx_blit_transparent(int x, int y, int w, int h, unsigned char *src, unsigned char transparent_color);
void gfx_fill_pattern(int x, int y, int w, int h, Pattern8x8 *pattern);
```

## Layer 3: Graphics Context

```c
typedef struct GraphicsContext {
    DisplayDriver *driver;
    int clip_x, clip_y, clip_w, clip_h;
    int translate_x, translate_y;
    enum { FILL_SOLID, FILL_PATTERN } fill_mode;
    unsigned char fg_color, bg_color;
    Pattern8x8 *current_pattern;
} GraphicsContext;

void gc_set_clip(GraphicsContext *gc, int x, int y, int w, int h);
void gc_translate(GraphicsContext *gc, int x, int y);
void gc_set_fill_pattern(GraphicsContext *gc, Pattern8x8 *pattern);
void gc_draw_rect(GraphicsContext *gc, int x, int y, int w, int h);
void gc_fill_rect(GraphicsContext *gc, int x, int y, int w, int h);
```

## Layer 4: Grid System

```c
typedef struct {
    int cell_width;   // 9 pixels
    int cell_height;  // 16 pixels
    int region_width; // 90 pixels
    int region_height;// 80 pixels
    int bar_width;    // 10 pixels
} GridConfig;

typedef struct {
    int col, row;     // Cell coordinates
    int cols, rows;   // Size in cells
} CellRect;

typedef struct {
    int x, y;         // Region coordinates (0-6, 0-5)
    int width, height;// Size in regions
} RegionRect;

void grid_cell_to_pixel(int col, int row, int *x, int *y);
void grid_region_to_pixel(int reg_x, int reg_y, int *x, int *y);
```

## Layer 5: Tile System

```c
typedef enum {
    TILE_9x16,   // Standard cell
    TILE_6x8,    // Dense text (min 2 cells)
    TILE_10x10,  // Small tiles (min 1 region)
    TILE_15x15   // Large tiles (min 3x16 cells)
} TileType;

typedef struct TileArea {
    TileType type;
    CellRect bounds;  // Area in cells
    union {
        Cell *cells;           // 9x16
        DenseChar *dense;      // 6x8
        Tile10 *tiles10;       // 10x10
        Tile15 *tiles15;       // 15x15
    } data;
} TileArea;

int tile_validate_area(TileType type, CellRect *area);
void tile_draw(TileArea *area, GraphicsContext *gc);
```

## Layer 6: View System

```c
typedef struct View {
    RegionRect bounds;           // Position in regions
    TileArea *tile_area;         // Optional tile content
    void (*draw)(struct View *self, GraphicsContext *gc);
    void (*update)(struct View *self, int delta_ms);
    int (*handle_input)(struct View *self, InputEvent *event);
    struct View *children;
    struct View *next;
    int needs_redraw;
    int z_order;
} View;

void view_invalidate(View *v);
void view_draw_tree(View *root, GraphicsContext *gc);
View *view_hit_test(View *root, int x, int y);
```

## Layer 7: Layout Manager

```c
typedef struct Layout {
    Region regions[6][7];
    Bar bar;
    View *active_view;
    enum { LAYOUT_SINGLE, LAYOUT_SPLIT, LAYOUT_CUSTOM } type;
} Layout;

typedef struct Region {
    View *content;
    enum { REGION_NAVIGATOR, REGION_TARGET, REGION_STANDALONE } role;
    struct Region *controls;  // For navigator->target relationship
} Region;

typedef struct Bar {
    int position;  // 0-7
    Cell cells[30];
} Bar;

void layout_set_bar_position(Layout *l, int position);
void layout_assign_view(Layout *l, int reg_x, int reg_y, View *v);
void layout_link_navigator(Layout *l, Region *nav, Region *target);
```

## Layer 8: Input System

```c
typedef struct InputState {
    enum { MODE_NORMAL, MODE_INSERT, MODE_VISUAL } vim_mode;
    int cursor_col, cursor_row;  // In cells
    View *focused_view;
    DragData *active_drag;
} InputState;

typedef struct InputEvent {
    enum { KEY_DOWN, KEY_UP, MOUSE_DOWN, MOUSE_UP, MOUSE_MOVE } type;
    union {
        struct { int key; char ascii; } keyboard;
        struct { int x, y, int button; } mouse;
    };
} InputEvent;

void input_dispatch(InputState *state, InputEvent *event, Layout *layout);
void input_move_focus(InputState *state, int direction);
```

## Layer 9: Main Loop

```c
typedef struct AppState {
    Layout layout;
    InputState input;
    GraphicsContext *gc;
    int running;
    unsigned int last_frame_ms;
} AppState;

void app_init(AppState *app);
void app_handle_event(AppState *app, InputEvent *event);
void app_update(AppState *app, int delta_ms);
void app_render(AppState *app);
void app_run(AppState *app);  // Main loop
```

This gives you clear separation of concerns with each layer building on the previous. The key insight is that your grid/region system sits in the middle, with lower layers dealing with pixels and higher layers dealing with logical layout.


Here are the key architecture and design decisions we’ve made:

## Display Architecture

- **640x480 resolution with 16-color palette** - Deliberately constrained for aesthetic consistency and themeability
- **Eventually support both VGA mode 12h and DISPI/VBE** - abstract under platform layer, for now focus on DISPI/VBE
- **Palette-indexed color model** - Use color indices (0-15) throughout API rather than RGB values
- **Function pointer dispatch for device drivers** - Allow runtime switching between VGA and DISPI implementations

## Grid and Layout System

- **9x16 pixel cells as standard unit** - Matches VGA text mode spacing, gives 71 columns plus 1 extra pixel
- **90x80 pixel regions** - Screen divided into 7x6 grid of regions
- **10-pixel “bar”** - Movable vertical strip that can be positioned between any column of regions
- **No scrolling** - Card/workspace metaphor instead of infinite scroll
- **Monospace fonts only** - Simplifies layout calculations and enforces grid alignment

## Tile System Constraints

- **Multiple tile sizes with minimum areas:**
  - 15x15 tiles: minimum 3x16 cells (half region wide, 3 regions tall)
  - 10x10 tiles: minimum 1 region (9x8 tiles)
  - 6x8 dense text: minimum 2 consecutive cells
  - 9x16 standard cells: any size

## Rendering Philosophy

- **Immediate mode API with internal state tracking** - Simple API but can optimize behind the scenes
- **Direct drawing through graphics context** - No intermediate pixel buffers for views
- **Dirty rectangle tracking** - Mark regions for redraw rather than full screen refresh
- **Pattern fills as first-class feature** - 8x8 tiled patterns for texture variety with limited colors

## UI Architecture

- **Everything is a View with hierarchy** - Composable components with parent-child relationships
- **Navigator-Target pane relationship** - Left pane controls right pane content
- **Tile-based “roguelike” navigation** - Spatial metaphor for file/code navigation
- **Vim-style modal input** - Global keyboard navigation with modes

## Animation Support

- **Hybrid event-driven and frame-based loop** - Idle efficiently but support 60fps when needed
- **Sprites integrated with views** - Not a separate system
- **Built for idle animations** - Designed from start to support animated UI elements

## Development Philosophy

- **Build reusable “mountain” of technology** - Graphics layer is foundation, not tied to specific application
- **Immediate code maintenance** - Fix issues now while code is simple
- **Clear layer separation** - Each layer has defined responsibilities and interfaces

The overall theme is deliberate constraints (grid-based, no scroll, monospace, 16 colors) that simplify implementation while enabling rich interaction through patterns, tiles, and spatial navigation.
