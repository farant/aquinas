/* Grid System for Aquinas OS UI
 * 
 * Based on 640x480 resolution divided into:
 * - Cells: 9x16 pixels (matching VGA text mode)
 * - Regions: 90x80 pixels (10x5 cells each)
 * - Screen: 7 regions wide, 6 regions tall
 * - Bar: 10 pixels wide, movable between columns
 */

#ifndef GRID_H
#define GRID_H

/* Grid dimensions */
#define SCREEN_WIDTH  640
#define SCREEN_HEIGHT 480

/* Cell dimensions (9x16 pixels) */
#define CELL_WIDTH    9
#define CELL_HEIGHT   16
#define CELLS_PER_ROW 71   /* 640 / 9 = 71.1, so 71 cells + 1 pixel */
#define CELLS_PER_COL 30   /* 480 / 16 = 30 cells exactly */

/* Region dimensions (90x80 pixels = 10x5 cells) */
#define REGION_WIDTH  90
#define REGION_HEIGHT 80
#define CELLS_PER_REGION_X 10
#define CELLS_PER_REGION_Y 5
#define REGIONS_PER_ROW 7  /* 640 / 90 = 7.1, with 10px for bar */
#define REGIONS_PER_COL 6  /* 480 / 80 = 6 regions exactly */

/* Bar dimensions */
#define BAR_WIDTH 10
#define BAR_HEIGHT SCREEN_HEIGHT

/* Grid configuration */
typedef struct {
    int cell_width;   /* 9 pixels */
    int cell_height;  /* 16 pixels */
    int region_width; /* 90 pixels */
    int region_height;/* 80 pixels */
    int bar_width;    /* 10 pixels */
    int bar_position; /* 0-7, which column the bar is after */
} GridConfig;

/* Cell coordinates */
typedef struct {
    int col;  /* 0-70 */
    int row;  /* 0-29 */
} CellCoord;

/* Region coordinates */
typedef struct {
    int x;    /* 0-6 */
    int y;    /* 0-5 */
} RegionCoord;

/* Cell rectangle (for tile areas) */
typedef struct {
    int col, row;     /* Top-left cell */
    int cols, rows;   /* Size in cells */
} CellRect;

/* Region rectangle */
typedef struct {
    int x, y;         /* Top-left region */
    int width, height;/* Size in regions */
} RegionRect;

/* Pixel coordinates */
typedef struct {
    int x, y;
} PixelCoord;

/* Initialize grid system */
void grid_init(void);

/* Coordinate conversion functions */
void grid_cell_to_pixel(int col, int row, int *x, int *y);
void grid_pixel_to_cell(int x, int y, int *col, int *row);
void grid_region_to_pixel(int reg_x, int reg_y, int *x, int *y);
void grid_pixel_to_region(int x, int y, int *reg_x, int *reg_y);
void grid_region_to_cells(int reg_x, int reg_y, int *col, int *row);
void grid_cell_to_region(int col, int row, int *reg_x, int *reg_y);

/* Bar management */
void grid_set_bar_position(int position);
int grid_get_bar_position(void);
void grid_get_bar_rect(int *x, int *y, int *width, int *height);
int grid_adjust_for_bar(int pixel_x);  /* Adjust x coordinate if past bar */

/* Cell rectangle operations */
int grid_cell_rect_to_pixels(CellRect *rect, int *x, int *y, int *w, int *h);
int grid_validate_cell_rect(CellRect *rect);

/* Region rectangle operations */
int grid_region_rect_to_pixels(RegionRect *rect, int *x, int *y, int *w, int *h);
int grid_validate_region_rect(RegionRect *rect);

/* Drawing helpers */
void grid_draw_cell_outline(int col, int row, unsigned char color);
void grid_draw_cell_filled(int col, int row, unsigned char color);
void grid_draw_region_outline(int reg_x, int reg_y, unsigned char color);
void grid_draw_grid_lines(unsigned char cell_color, unsigned char region_color);

/* Hit testing */
int grid_point_in_cell(int px, int py, int col, int row);
int grid_point_in_region(int px, int py, int reg_x, int reg_y);

#endif /* GRID_H */