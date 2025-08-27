/* Graphics Context implementation
 *
 * This layer sits on top of the Display Driver interface and provides
 * stateful graphics operations including clipping, translation, and pattern fills.
 * 
 * Key features:
 * - Clipping: All drawing operations are automatically clipped to bounds
 * - Translation: Coordinates are automatically translated by offset
 * - Pattern fills: Support for 8x8 tiled patterns
 * - Integration with DISPI: Uses existing DISPI primitives under the hood
 */

#include "graphics_context.h"
#include "memory.h"
#include "dispi.h"
#include <stddef.h>

/* Create a new graphics context */
GraphicsContext* gc_create(DisplayDriver *driver) {
    GraphicsContext *gc = (GraphicsContext*)malloc(sizeof(GraphicsContext));
    if (gc) {
        gc_init(gc, driver);
    }
    return gc;
}

/* Destroy a graphics context */
void gc_destroy(GraphicsContext *gc) {
    if (gc) {
        free(gc);
    }
}

/* Initialize a graphics context */
void gc_init(GraphicsContext *gc, DisplayDriver *driver) {
    if (!gc) return;
    
    gc->driver = driver;
    
    /* Initialize to full screen bounds */
    gc_clear_clip(gc);
    
    /* No translation by default */
    gc->translate_x = 0;
    gc->translate_y = 0;
    
    /* Default colors and fill mode */
    gc->fg_color = 15;  /* White */
    gc->bg_color = 0;   /* Black */
    gc->fill_mode = FILL_SOLID;
    gc->current_pattern = NULL;
}

/* Set clipping bounds */
void gc_set_clip(GraphicsContext *gc, int x, int y, int w, int h) {
    if (!gc) return;
    
    /* Clamp to screen bounds */
    if (x < 0) {
        w += x;
        x = 0;
    }
    if (y < 0) {
        h += y;
        y = 0;
    }
    
    if (x + w > gc->driver->width) {
        w = gc->driver->width - x;
    }
    if (y + h > gc->driver->height) {
        h = gc->driver->height - y;
    }
    
    /* Ensure valid bounds */
    if (w < 0) w = 0;
    if (h < 0) h = 0;
    
    gc->clip_x = x;
    gc->clip_y = y;
    gc->clip_w = w;
    gc->clip_h = h;
}

/* Get current clipping bounds */
void gc_get_clip(GraphicsContext *gc, int *x, int *y, int *w, int *h) {
    if (!gc) return;
    
    if (x) *x = gc->clip_x;
    if (y) *y = gc->clip_y;
    if (w) *w = gc->clip_w;
    if (h) *h = gc->clip_h;
}

/* Clear clipping (reset to full screen) */
void gc_clear_clip(GraphicsContext *gc) {
    if (!gc || !gc->driver) return;
    
    gc->clip_x = 0;
    gc->clip_y = 0;
    gc->clip_w = gc->driver->width;
    gc->clip_h = gc->driver->height;
}

/* Set translation offset */
void gc_set_translation(GraphicsContext *gc, int x, int y) {
    if (!gc) return;
    
    gc->translate_x = x;
    gc->translate_y = y;
}

/* Get current translation */
void gc_get_translation(GraphicsContext *gc, int *x, int *y) {
    if (!gc) return;
    
    if (x) *x = gc->translate_x;
    if (y) *y = gc->translate_y;
}

/* Add to current translation */
void gc_translate(GraphicsContext *gc, int dx, int dy) {
    if (!gc) return;
    
    gc->translate_x += dx;
    gc->translate_y += dy;
}

/* Set both foreground and background colors */
void gc_set_colors(GraphicsContext *gc, unsigned char fg, unsigned char bg) {
    if (!gc) return;
    
    gc->fg_color = fg;
    gc->bg_color = bg;
}

/* Set foreground color */
void gc_set_fg_color(GraphicsContext *gc, unsigned char color) {
    if (!gc) return;
    
    gc->fg_color = color;
}

/* Set background color */
void gc_set_bg_color(GraphicsContext *gc, unsigned char color) {
    if (!gc) return;
    
    gc->bg_color = color;
}

/* Set fill mode */
void gc_set_fill_mode(GraphicsContext *gc, int mode) {
    if (!gc) return;
    
    gc->fill_mode = mode;
}

/* Set current pattern */
void gc_set_pattern(GraphicsContext *gc, Pattern8x8 *pattern) {
    if (!gc) return;
    
    gc->current_pattern = pattern;
}

/* Apply translation to coordinates */
void gc_apply_translation(GraphicsContext *gc, int *x, int *y) {
    if (!gc || !x || !y) return;
    
    *x += gc->translate_x;
    *y += gc->translate_y;
}

/* Clip a rectangle to the context bounds 
   Returns 1 if rectangle is visible, 0 if completely clipped */
int gc_clip_rect(GraphicsContext *gc, int *x, int *y, int *w, int *h) {
    int x1, y1, x2, y2;
    if (!gc || !x || !y || !w || !h) return 0;
    
    x1 = *x;
    y1 = *y;
    x2 = *x + *w;
    y2 = *y + *h;
    
    /* Clip to context bounds */
    if (x1 < gc->clip_x) x1 = gc->clip_x;
    if (y1 < gc->clip_y) y1 = gc->clip_y;
    if (x2 > gc->clip_x + gc->clip_w) x2 = gc->clip_x + gc->clip_w;
    if (y2 > gc->clip_y + gc->clip_h) y2 = gc->clip_y + gc->clip_h;
    
    /* Check if still valid */
    if (x1 >= x2 || y1 >= y2) {
        return 0;  /* Completely clipped */
    }
    
    *x = x1;
    *y = y1;
    *w = x2 - x1;
    *h = y2 - y1;
    
    return 1;  /* Visible */
}

/* Simple line clipping using Cohen-Sutherland algorithm */
int gc_clip_line(GraphicsContext *gc, int *x0, int *y0, int *x1, int *y1) {
    const int INSIDE = 0; /* 0000 */
    const int LEFT = 1;   /* 0001 */
    const int RIGHT = 2;  /* 0010 */
    const int BOTTOM = 4; /* 0100 */
    const int TOP = 8;    /* 1000 */
    int xmin, ymin, xmax, ymax;
    int code0, code1;
    
    if (!gc || !x0 || !y0 || !x1 || !y1) return 0;
    
    xmin = gc->clip_x;
    ymin = gc->clip_y;
    xmax = gc->clip_x + gc->clip_w - 1;
    ymax = gc->clip_y + gc->clip_h - 1;
    
    /* Compute region codes inline */
    code0 = INSIDE;
    if (*x0 < xmin) code0 |= LEFT;
    else if (*x0 > xmax) code0 |= RIGHT;
    if (*y0 < ymin) code0 |= BOTTOM;
    else if (*y0 > ymax) code0 |= TOP;
    
    code1 = INSIDE;
    if (*x1 < xmin) code1 |= LEFT;
    else if (*x1 > xmax) code1 |= RIGHT;
    if (*y1 < ymin) code1 |= BOTTOM;
    else if (*y1 > ymax) code1 |= TOP;
    
    while (1) {
        if (!(code0 | code1)) {
            /* Both endpoints inside window */
            return 1;
        } else if (code0 & code1) {
            /* Both endpoints share an outside zone */
            return 0;
        } else {
            /* Some segment of line lies within the window */
            int code_out;
            int x, y;
            
            /* Pick the outside point */
            if (code0 != 0) {
                code_out = code0;
            } else {
                code_out = code1;
            }
            
            /* Find intersection point */
            if (code_out & TOP) {
                x = *x0 + (*x1 - *x0) * (ymax - *y0) / (*y1 - *y0);
                y = ymax;
            } else if (code_out & BOTTOM) {
                x = *x0 + (*x1 - *x0) * (ymin - *y0) / (*y1 - *y0);
                y = ymin;
            } else if (code_out & RIGHT) {
                y = *y0 + (*y1 - *y0) * (xmax - *x0) / (*x1 - *x0);
                x = xmax;
            } else if (code_out & LEFT) {
                y = *y0 + (*y1 - *y0) * (xmin - *x0) / (*x1 - *x0);
                x = xmin;
            }
            
            /* Replace the outside point with intersection */
            if (code_out == code0) {
                *x0 = x;
                *y0 = y;
                /* Recompute code0 */
                code0 = INSIDE;
                if (*x0 < xmin) code0 |= LEFT;
                else if (*x0 > xmax) code0 |= RIGHT;
                if (*y0 < ymin) code0 |= BOTTOM;
                else if (*y0 > ymax) code0 |= TOP;
            } else {
                *x1 = x;
                *y1 = y;
                /* Recompute code1 */
                code1 = INSIDE;
                if (*x1 < xmin) code1 |= LEFT;
                else if (*x1 > xmax) code1 |= RIGHT;
                if (*y1 < ymin) code1 |= BOTTOM;
                else if (*y1 > ymax) code1 |= TOP;
            }
        }
    }
}

/* Check if a point is within the clipping bounds */
static int gc_point_visible(GraphicsContext *gc, int x, int y) {
    if (!gc) return 0;
    
    return (x >= gc->clip_x && x < gc->clip_x + gc->clip_w &&
            y >= gc->clip_y && y < gc->clip_y + gc->clip_h);
}

/* Context-aware drawing functions */

/* Set a pixel with context transformation and clipping */
void gc_set_pixel(GraphicsContext *gc, int x, int y, unsigned char color) {
    if (!gc || !gc->driver) return;
    
    /* Apply translation */
    gc_apply_translation(gc, &x, &y);
    
    /* Check clipping */
    if (!gc_point_visible(gc, x, y)) {
        return;
    }
    
    /* Draw the pixel */
    gc->driver->set_pixel(x, y, color);
}

/* Get a pixel with context transformation and clipping */
unsigned char gc_get_pixel(GraphicsContext *gc, int x, int y) {
    if (!gc || !gc->driver) return 0;
    
    /* Apply translation */
    gc_apply_translation(gc, &x, &y);
    
    /* Check clipping */
    if (!gc_point_visible(gc, x, y)) {
        return 0;
    }
    
    return gc->driver->get_pixel(x, y);
}

/* Draw a line with context transformation and clipping */
void gc_draw_line(GraphicsContext *gc, int x0, int y0, int x1, int y1, unsigned char color) {
    if (!gc || !gc->driver) return;
    
    /* Apply translation */
    gc_apply_translation(gc, &x0, &y0);
    gc_apply_translation(gc, &x1, &y1);
    
    /* Clip the line */
    if (!gc_clip_line(gc, &x0, &y0, &x1, &y1)) {
        return;  /* Line is completely outside clip bounds */
    }
    
    /* Use DISPI line drawing function */
    dispi_draw_line(x0, y0, x1, y1, color);
}

/* Draw a rectangle outline with context transformation and clipping */
void gc_draw_rect(GraphicsContext *gc, int x, int y, int w, int h, unsigned char color) {
    if (!gc || !gc->driver) return;
    
    /* Apply translation */
    gc_apply_translation(gc, &x, &y);
    
    /* Clip the rectangle */
    if (!gc_clip_rect(gc, &x, &y, &w, &h)) {
        return;  /* Rectangle is completely outside clip bounds */
    }
    
    /* Draw four lines to form the rectangle outline */
    if (w > 0 && h > 0) {
        /* Top edge */
        dispi_draw_line(x, y, x + w - 1, y, color);
        /* Bottom edge */
        if (h > 1) {
            dispi_draw_line(x, y + h - 1, x + w - 1, y + h - 1, color);
        }
        /* Left edge */
        if (h > 1) {
            dispi_draw_line(x, y + 1, x, y + h - 2, color);
        }
        /* Right edge */
        if (w > 1 && h > 1) {
            dispi_draw_line(x + w - 1, y + 1, x + w - 1, y + h - 2, color);
        }
    }
}

/* Fill a rectangle with solid color, respecting context transformation and clipping */
void gc_fill_rect(GraphicsContext *gc, int x, int y, int w, int h, unsigned char color) {
    if (!gc || !gc->driver) return;
    
    /* Apply translation */
    gc_apply_translation(gc, &x, &y);
    
    /* Clip the rectangle */
    if (!gc_clip_rect(gc, &x, &y, &w, &h)) {
        return;  /* Rectangle is completely outside clip bounds */
    }
    
    /* Use driver fill rect function */
    gc->driver->fill_rect(x, y, w, h, color);
}

/* Fill a rectangle with a pattern */
void gc_fill_rect_pattern(GraphicsContext *gc, int x, int y, int w, int h, Pattern8x8 *pattern) {
    int orig_x, orig_y;
    int dy, dx;
    if (!gc || !gc->driver || !pattern) return;
    
    orig_x = x;
    orig_y = y;
    
    /* Apply translation */
    gc_apply_translation(gc, &x, &y);
    
    /* Clip the rectangle */
    if (!gc_clip_rect(gc, &x, &y, &w, &h)) {
        return;  /* Rectangle is completely outside clip bounds */
    }
    
    /* Draw pattern pixel by pixel */
    for (dy = 0; dy < h; dy++) {
        for (dx = 0; dx < w; dx++) {
            int px = x + dx;
            int py = y + dy;
            
            /* Calculate pattern coordinates (relative to original position) */
            int pattern_x = (orig_x + dx) & 7;  /* mod 8 */
            int pattern_y = (orig_y + dy) & 7;  /* mod 8 */
            
            /* Check if this pixel should be foreground or background */
            unsigned char row = pattern->rows[pattern_y];
            int bit = (row >> (7 - pattern_x)) & 1;
            
            unsigned char color = bit ? gc->fg_color : gc->bg_color;
            gc->driver->set_pixel(px, py, color);
        }
    }
}

/* Fill a rectangle using the current context pattern */
void gc_fill_rect_current_pattern(GraphicsContext *gc, int x, int y, int w, int h) {
    if (!gc) return;
    
    if (gc->fill_mode == FILL_PATTERN && gc->current_pattern) {
        gc_fill_rect_pattern(gc, x, y, w, h, gc->current_pattern);
    } else {
        gc_fill_rect(gc, x, y, w, h, gc->fg_color);
    }
}

/* Draw a circle with context transformation and clipping */
void gc_draw_circle(GraphicsContext *gc, int cx, int cy, int radius, unsigned char color) {
    if (!gc || !gc->driver) return;
    
    /* Apply translation */
    gc_apply_translation(gc, &cx, &cy);
    
    /* Simple bounds check - if circle center is too far outside clip bounds, skip */
    if (cx + radius < gc->clip_x || cx - radius >= gc->clip_x + gc->clip_w ||
        cy + radius < gc->clip_y || cy - radius >= gc->clip_y + gc->clip_h) {
        return;
    }
    
    /* Use DISPI circle drawing function (it will handle individual pixel clipping) */
    dispi_draw_circle(cx, cy, radius, color);
}

/* Fill a circle with context transformation and clipping */
void gc_fill_circle(GraphicsContext *gc, int cx, int cy, int radius, unsigned char color) {
    /* Fill circle using midpoint algorithm */
    int x = 0;
    int y = radius;
    int d = 1 - radius;
    int span_y, px, x1, x2;
    
    if (!gc || !gc->driver || radius < 0) return;
    
    /* Apply translation */
    gc_apply_translation(gc, &cx, &cy);
    
    /* Simple bounds check */
    if (cx + radius < gc->clip_x || cx - radius >= gc->clip_x + gc->clip_w ||
        cy + radius < gc->clip_y || cy - radius >= gc->clip_y + gc->clip_h) {
        return;
    }
    
    while (x <= y) {
        /* Draw horizontal lines for each octant */
        
        /* Top half */
        int y1 = cy - y;
        int y2 = cy - x;
        
        /* Bottom half */
        int y3 = cy + x;
        int y4 = cy + y;
        
        /* Draw horizontal spans, clipped to context bounds */
        for (span_y = y1; span_y <= y1; span_y++) {
            if (span_y >= gc->clip_y && span_y < gc->clip_y + gc->clip_h) {
                x1 = cx - x;
                x2 = cx + x;
                if (x1 < gc->clip_x) x1 = gc->clip_x;
                if (x2 >= gc->clip_x + gc->clip_w) x2 = gc->clip_x + gc->clip_w - 1;
                for (px = x1; px <= x2; px++) {
                    gc->driver->set_pixel(px, span_y, color);
                }
            }
        }
        
        if (y2 != y1) {
            for (span_y = y2; span_y <= y2; span_y++) {
                if (span_y >= gc->clip_y && span_y < gc->clip_y + gc->clip_h) {
                    x1 = cx - y;
                    x2 = cx + y;
                    if (x1 < gc->clip_x) x1 = gc->clip_x;
                    if (x2 >= gc->clip_x + gc->clip_w) x2 = gc->clip_x + gc->clip_w - 1;
                    for (px = x1; px <= x2; px++) {
                        gc->driver->set_pixel(px, span_y, color);
                    }
                }
            }
        }
        
        if (y3 != y2) {
            for (span_y = y3; span_y <= y3; span_y++) {
                if (span_y >= gc->clip_y && span_y < gc->clip_y + gc->clip_h) {
                    x1 = cx - y;
                    x2 = cx + y;
                    if (x1 < gc->clip_x) x1 = gc->clip_x;
                    if (x2 >= gc->clip_x + gc->clip_w) x2 = gc->clip_x + gc->clip_w - 1;
                    for (px = x1; px <= x2; px++) {
                        gc->driver->set_pixel(px, span_y, color);
                    }
                }
            }
        }
        
        if (y4 != y3) {
            for (span_y = y4; span_y <= y4; span_y++) {
                if (span_y >= gc->clip_y && span_y < gc->clip_y + gc->clip_h) {
                    x1 = cx - x;
                    x2 = cx + x;
                    if (x1 < gc->clip_x) x1 = gc->clip_x;
                    if (x2 >= gc->clip_x + gc->clip_w) x2 = gc->clip_x + gc->clip_w - 1;
                    for (px = x1; px <= x2; px++) {
                        gc->driver->set_pixel(px, span_y, color);
                    }
                }
            }
        }
        
        if (d < 0) {
            d += 2 * x + 3;
        } else {
            d += 2 * (x - y) + 5;
            y--;
        }
        x++;
    }
}

/* Pattern utility functions */

/* Create a solid pattern (all foreground or all background) */
void pattern_create_solid(Pattern8x8 *pattern, int fill) {
    unsigned char value;
    int i;
    if (!pattern) return;
    
    value = fill ? 0xFF : 0x00;
    for (i = 0; i < 8; i++) {
        pattern->rows[i] = value;
    }
}

/* Create a classic checkerboard pattern */
void pattern_create_checkerboard(Pattern8x8 *pattern) {
    int i;
    if (!pattern) return;
    
    /* Alternating pattern: 0xAA = 10101010, 0x55 = 01010101 */
    for (i = 0; i < 8; i++) {
        pattern->rows[i] = (i & 1) ? 0x55 : 0xAA;
    }
}

/* Create horizontal stripe pattern */
void pattern_create_horizontal_stripes(Pattern8x8 *pattern, int width) {
    int i;
    if (!pattern || width <= 0 || width > 8) return;
    
    for (i = 0; i < 8; i++) {
        pattern->rows[i] = ((i / width) & 1) ? 0xFF : 0x00;
    }
}

/* Create vertical stripe pattern */
void pattern_create_vertical_stripes(Pattern8x8 *pattern, int width) {
    unsigned char value;
    int bit, i;
    if (!pattern || width <= 0 || width > 8) return;
    
    value = 0;
    for (bit = 0; bit < 8; bit++) {
        if ((bit / width) & 1) {
            value |= (1 << (7 - bit));
        }
    }
    
    for (i = 0; i < 8; i++) {
        pattern->rows[i] = value;
    }
}

/* Create diagonal stripe pattern */
void pattern_create_diagonal(Pattern8x8 *pattern) {
    int i;
    if (!pattern) return;
    
    for (i = 0; i < 8; i++) {
        /* Create diagonal stripes */
        pattern->rows[i] = (0x81 >> (i % 8)) | (0x81 << (8 - (i % 8)));
    }
}

/* Create dot pattern */
void pattern_create_dots(Pattern8x8 *pattern, int spacing) {
    int i, y, x;
    if (!pattern || spacing <= 0 || spacing > 4) return;
    
    for (i = 0; i < 8; i++) {
        pattern->rows[i] = 0;
    }
    
    /* Place dots at regular intervals */
    for (y = 0; y < 8; y += spacing) {
        for (x = 0; x < 8; x += spacing) {
            pattern->rows[y] |= (1 << (7 - x));
        }
    }
}