/* Grid System Implementation
 * 
 * Provides coordinate mapping between pixels, cells, and regions
 * for the Aquinas OS UI system.
 */

#include "grid.h"
#include "display_driver.h"
#include "dispi.h"
#include "serial.h"

/* Global grid configuration */
static GridConfig grid_config = {
    CELL_WIDTH,    /* 9 pixels */
    CELL_HEIGHT,   /* 16 pixels */
    REGION_WIDTH,  /* 90 pixels */
    REGION_HEIGHT, /* 80 pixels */
    BAR_WIDTH,     /* 10 pixels */
    3              /* Default bar position after column 3 */
};

/* Initialize grid system */
void grid_init(void) {
    serial_write_string("Grid system initialized: ");
    serial_write_string("Cells=");
    serial_write_hex(CELLS_PER_ROW);
    serial_write_string("x");
    serial_write_hex(CELLS_PER_COL);
    serial_write_string(", Regions=");
    serial_write_hex(REGIONS_PER_ROW);
    serial_write_string("x");
    serial_write_hex(REGIONS_PER_COL);
    serial_write_string("\n");
}

/* Convert cell coordinates to pixel coordinates */
void grid_cell_to_pixel(int col, int row, int *x, int *y) {
    if (x) *x = col * CELL_WIDTH;
    if (y) *y = row * CELL_HEIGHT;
}

/* Convert pixel coordinates to cell coordinates */
void grid_pixel_to_cell(int x, int y, int *col, int *row) {
    if (col) *col = x / CELL_WIDTH;
    if (row) *row = y / CELL_HEIGHT;
}

/* Convert region coordinates to pixel coordinates */
void grid_region_to_pixel(int reg_x, int reg_y, int *x, int *y) {
    int px = reg_x * REGION_WIDTH;
    
    /* Adjust for bar if it's before this region */
    /* Bar position 3 means bar is after column 3, before column 4 */
    if (grid_config.bar_position >= 0 && reg_x > grid_config.bar_position) {
        px += BAR_WIDTH;
    }
    
    if (x) *x = px;
    if (y) *y = reg_y * REGION_HEIGHT;
}

/* Convert pixel coordinates to region coordinates */
void grid_pixel_to_region(int x, int y, int *reg_x, int *reg_y) {
    int adjusted_x = x;
    
    /* Adjust for bar position */
    if (grid_config.bar_position >= 0) {
        int bar_x = (grid_config.bar_position + 1) * REGION_WIDTH;
        if (x >= bar_x && x < bar_x + BAR_WIDTH) {
            /* Pixel is in the bar itself */
            if (reg_x) *reg_x = -1;  /* Invalid region */
            if (reg_y) *reg_y = -1;
            return;
        } else if (x >= bar_x + BAR_WIDTH) {
            /* Pixel is after the bar */
            adjusted_x -= BAR_WIDTH;
        }
    }
    
    if (reg_x) *reg_x = adjusted_x / REGION_WIDTH;
    if (reg_y) *reg_y = y / REGION_HEIGHT;
}

/* Convert region to its starting cell */
void grid_region_to_cells(int reg_x, int reg_y, int *col, int *row) {
    if (col) *col = reg_x * CELLS_PER_REGION_X;
    if (row) *row = reg_y * CELLS_PER_REGION_Y;
}

/* Convert cell to its containing region */
void grid_cell_to_region(int col, int row, int *reg_x, int *reg_y) {
    if (reg_x) *reg_x = col / CELLS_PER_REGION_X;
    if (reg_y) *reg_y = row / CELLS_PER_REGION_Y;
}

/* Set bar position (0-7, or -1 for no bar) */
void grid_set_bar_position(int position) {
    if (position >= -1 && position < REGIONS_PER_ROW) {
        grid_config.bar_position = position;
    }
}

/* Get current bar position */
int grid_get_bar_position(void) {
    return grid_config.bar_position;
}

/* Get bar rectangle in pixels */
void grid_get_bar_rect(int *x, int *y, int *width, int *height) {
    if (grid_config.bar_position >= 0) {
        if (x) *x = (grid_config.bar_position + 1) * REGION_WIDTH;
        if (y) *y = 0;
        if (width) *width = BAR_WIDTH;
        if (height) *height = BAR_HEIGHT;
    } else {
        /* No bar */
        if (x) *x = -1;
        if (y) *y = -1;
        if (width) *width = 0;
        if (height) *height = 0;
    }
}

/* Adjust x coordinate if it's past the bar */
int grid_adjust_for_bar(int pixel_x) {
    if (grid_config.bar_position >= 0) {
        int bar_x = (grid_config.bar_position + 1) * REGION_WIDTH;
        if (pixel_x >= bar_x) {
            return pixel_x + BAR_WIDTH;
        }
    }
    return pixel_x;
}

/* Convert cell rectangle to pixel coordinates */
int grid_cell_rect_to_pixels(CellRect *rect, int *x, int *y, int *w, int *h) {
    if (!rect) return 0;
    
    grid_cell_to_pixel(rect->col, rect->row, x, y);
    if (w) *w = rect->cols * CELL_WIDTH;
    if (h) *h = rect->rows * CELL_HEIGHT;
    
    return 1;
}

/* Validate cell rectangle bounds */
int grid_validate_cell_rect(CellRect *rect) {
    if (!rect) return 0;
    
    /* Check if starting position is valid */
    if (rect->col < 0 || rect->col >= CELLS_PER_ROW) return 0;
    if (rect->row < 0 || rect->row >= CELLS_PER_COL) return 0;
    
    /* Check if size is valid */
    if (rect->cols <= 0 || rect->rows <= 0) return 0;
    
    /* Check if it fits on screen */
    if (rect->col + rect->cols > CELLS_PER_ROW) return 0;
    if (rect->row + rect->rows > CELLS_PER_COL) return 0;
    
    return 1;
}

/* Convert region rectangle to pixel coordinates */
int grid_region_rect_to_pixels(RegionRect *rect, int *x, int *y, int *w, int *h) {
    if (!rect) return 0;
    
    grid_region_to_pixel(rect->x, rect->y, x, y);
    if (w) *w = rect->width * REGION_WIDTH;
    if (h) *h = rect->height * REGION_HEIGHT;
    
    return 1;
}

/* Validate region rectangle bounds */
int grid_validate_region_rect(RegionRect *rect) {
    if (!rect) return 0;
    
    /* Check if starting position is valid */
    if (rect->x < 0 || rect->x >= REGIONS_PER_ROW) return 0;
    if (rect->y < 0 || rect->y >= REGIONS_PER_COL) return 0;
    
    /* Check if size is valid */
    if (rect->width <= 0 || rect->height <= 0) return 0;
    
    /* Check if it fits on screen */
    if (rect->x + rect->width > REGIONS_PER_ROW) return 0;
    if (rect->y + rect->height > REGIONS_PER_COL) return 0;
    
    return 1;
}

/* Draw outline around a cell */
void grid_draw_cell_outline(int col, int row, unsigned char color) {
    int x, y;
    int cell_region;
    DisplayDriver *driver = display_get_driver();
    
    if (!driver) return;
    
    grid_cell_to_pixel(col, row, &x, &y);
    
    /* Account for bar if cell is in a region after it */
    cell_region = col / CELLS_PER_REGION_X;
    if (grid_config.bar_position >= 0 && cell_region > grid_config.bar_position) {
        x += BAR_WIDTH;
    }
    
    /* Draw rectangle outline */
    dispi_draw_line(x, y, x + CELL_WIDTH - 1, y, color);  /* Top */
    dispi_draw_line(x, y + CELL_HEIGHT - 1, x + CELL_WIDTH - 1, y + CELL_HEIGHT - 1, color);  /* Bottom */
    dispi_draw_line(x, y, x, y + CELL_HEIGHT - 1, color);  /* Left */
    dispi_draw_line(x + CELL_WIDTH - 1, y, x + CELL_WIDTH - 1, y + CELL_HEIGHT - 1, color);  /* Right */
}

/* Fill a cell with color */
void grid_draw_cell_filled(int col, int row, unsigned char color) {
    int x, y;
    int cell_region;
    DisplayDriver *driver = display_get_driver();
    
    if (!driver) return;
    
    grid_cell_to_pixel(col, row, &x, &y);
    
    /* Account for bar if cell is in a region after it */
    cell_region = col / CELLS_PER_REGION_X;
    if (grid_config.bar_position >= 0 && cell_region > grid_config.bar_position) {
        x += BAR_WIDTH;
    }
    
    /* Fill the cell */
    display_fill_rect(x, y, CELL_WIDTH, CELL_HEIGHT, color);
}

/* Draw outline around a region */
void grid_draw_region_outline(int reg_x, int reg_y, unsigned char color) {
    int x, y;
    DisplayDriver *driver = display_get_driver();
    
    if (!driver) return;
    
    grid_region_to_pixel(reg_x, reg_y, &x, &y);
    
    /* Draw thicker rectangle outline (2 pixels wide) */
    dispi_draw_line(x, y, x + REGION_WIDTH - 1, y, color);  /* Top */
    dispi_draw_line(x, y + 1, x + REGION_WIDTH - 1, y + 1, color);  /* Top inner */
    
    dispi_draw_line(x, y + REGION_HEIGHT - 1, x + REGION_WIDTH - 1, y + REGION_HEIGHT - 1, color);  /* Bottom */
    dispi_draw_line(x, y + REGION_HEIGHT - 2, x + REGION_WIDTH - 1, y + REGION_HEIGHT - 2, color);  /* Bottom inner */
    
    dispi_draw_line(x, y, x, y + REGION_HEIGHT - 1, color);  /* Left */
    dispi_draw_line(x + 1, y, x + 1, y + REGION_HEIGHT - 1, color);  /* Left inner */
    
    dispi_draw_line(x + REGION_WIDTH - 1, y, x + REGION_WIDTH - 1, y + REGION_HEIGHT - 1, color);  /* Right */
    dispi_draw_line(x + REGION_WIDTH - 2, y, x + REGION_WIDTH - 2, y + REGION_HEIGHT - 1, color);  /* Right inner */
}

/* Draw grid lines for visualization */
void grid_draw_grid_lines(unsigned char cell_color, unsigned char region_color) {
    int i, x, y;
    DisplayDriver *driver = display_get_driver();
    
    if (!driver) return;
    
    /* Draw cell grid lines (vertical) */
    for (i = 1; i < CELLS_PER_ROW; i++) {
        /* Calculate actual x position based on which region this cell is in */
        int cell_region = i / CELLS_PER_REGION_X;
        int cell_in_region = i % CELLS_PER_REGION_X;
        int region_x, region_y;
        
        /* Get the pixel position of this region */
        grid_region_to_pixel(cell_region, 0, &region_x, &region_y);
        
        /* Calculate actual x position of this cell line */
        x = region_x + (cell_in_region * CELL_WIDTH);
        
        /* Skip if this would be on a region boundary (but not if shifted by bar) */
        if (cell_in_region != 0) {
            dispi_draw_line(x, 0, x, SCREEN_HEIGHT - 1, cell_color);
        }
    }
    
    /* Draw cell grid lines (horizontal) */
    for (i = 1; i < CELLS_PER_COL; i++) {
        y = i * CELL_HEIGHT;
        /* Skip if this would be on a region boundary */
        if (y % REGION_HEIGHT != 0) {
            dispi_draw_line(0, y, SCREEN_WIDTH - 1, y, cell_color);
        }
    }
    
    /* Draw region grid lines (vertical) - thicker */
    for (i = 0; i < REGIONS_PER_ROW; i++) {
        /* Get actual pixel position for this region column */
        int px, py;
        grid_region_to_pixel(i, 0, &px, &py);
        
        /* Draw the left edge of this region (except for first) */
        if (i > 0) {
            dispi_draw_line(px, 0, px, SCREEN_HEIGHT - 1, region_color);
            dispi_draw_line(px + 1, 0, px + 1, SCREEN_HEIGHT - 1, region_color);
        }
        
        /* Draw the right edge of the last region */
        if (i == REGIONS_PER_ROW - 1) {
            dispi_draw_line(px + REGION_WIDTH, 0, px + REGION_WIDTH, SCREEN_HEIGHT - 1, region_color);
            dispi_draw_line(px + REGION_WIDTH - 1, 0, px + REGION_WIDTH - 1, SCREEN_HEIGHT - 1, region_color);
        }
    }
    
    /* Draw region grid lines (horizontal) - thicker */
    for (i = 1; i < REGIONS_PER_COL; i++) {
        y = i * REGION_HEIGHT;
        dispi_draw_line(0, y, SCREEN_WIDTH - 1, y, region_color);
        dispi_draw_line(0, y + 1, SCREEN_WIDTH - 1, y + 1, region_color);
    }
    
    /* Draw bar if present */
    if (grid_config.bar_position >= 0) {
        int bar_x, bar_y, bar_w, bar_h;
        grid_get_bar_rect(&bar_x, &bar_y, &bar_w, &bar_h);
        display_fill_rect(bar_x, bar_y, bar_w, bar_h, 11);  /* Gold color for bar */
    }
}

/* Check if point is in cell */
int grid_point_in_cell(int px, int py, int col, int row) {
    int cell_x, cell_y;
    grid_cell_to_pixel(col, row, &cell_x, &cell_y);
    
    return (px >= cell_x && px < cell_x + CELL_WIDTH &&
            py >= cell_y && py < cell_y + CELL_HEIGHT);
}

/* Check if point is in region */
int grid_point_in_region(int px, int py, int reg_x, int reg_y) {
    int region_x, region_y;
    grid_region_to_pixel(reg_x, reg_y, &region_x, &region_y);
    
    return (px >= region_x && px < region_x + REGION_WIDTH &&
            py >= region_y && py < region_y + REGION_HEIGHT);
}