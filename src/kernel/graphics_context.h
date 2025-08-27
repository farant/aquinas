#ifndef GRAPHICS_CONTEXT_H
#define GRAPHICS_CONTEXT_H

#include "display_driver.h"

/* 8x8 pattern for fills - each row is represented as a byte where
 * bit 7 = leftmost pixel, bit 0 = rightmost pixel
 * 1 = foreground color, 0 = background color
 */
typedef struct {
    unsigned char rows[8];
} Pattern8x8;

/* Graphics context - maintains drawing state including clipping and translation */
typedef struct GraphicsContext {
    DisplayDriver *driver;
    
    /* Clipping bounds - drawing operations are restricted to this rectangle */
    int clip_x, clip_y, clip_w, clip_h;
    
    /* Translation offset - all coordinates are offset by this amount */
    int translate_x, translate_y;
    
    /* Fill mode and colors */
    enum { FILL_SOLID, FILL_PATTERN } fill_mode;
    unsigned char fg_color;
    unsigned char bg_color;
    
    /* Current pattern for pattern fills */
    Pattern8x8 *current_pattern;
} GraphicsContext;

/* Context lifecycle functions */
GraphicsContext* gc_create(DisplayDriver *driver);
void gc_destroy(GraphicsContext *gc);
void gc_init(GraphicsContext *gc, DisplayDriver *driver);

/* Context state management */
void gc_set_clip(GraphicsContext *gc, int x, int y, int w, int h);
void gc_get_clip(GraphicsContext *gc, int *x, int *y, int *w, int *h);
void gc_clear_clip(GraphicsContext *gc);  /* Reset to full screen */

void gc_set_translation(GraphicsContext *gc, int x, int y);
void gc_get_translation(GraphicsContext *gc, int *x, int *y);
void gc_translate(GraphicsContext *gc, int dx, int dy);  /* Add to current translation */

void gc_set_colors(GraphicsContext *gc, unsigned char fg, unsigned char bg);
void gc_set_fg_color(GraphicsContext *gc, unsigned char color);
void gc_set_bg_color(GraphicsContext *gc, unsigned char color);

void gc_set_fill_mode(GraphicsContext *gc, int mode);  /* FILL_SOLID or FILL_PATTERN */
void gc_set_pattern(GraphicsContext *gc, Pattern8x8 *pattern);

/* Context-aware drawing functions */
/* These functions respect clipping bounds and apply translation automatically */
void gc_set_pixel(GraphicsContext *gc, int x, int y, unsigned char color);
unsigned char gc_get_pixel(GraphicsContext *gc, int x, int y);

void gc_draw_line(GraphicsContext *gc, int x0, int y0, int x1, int y1, unsigned char color);
void gc_draw_rect(GraphicsContext *gc, int x, int y, int w, int h, unsigned char color);
void gc_fill_rect(GraphicsContext *gc, int x, int y, int w, int h, unsigned char color);
void gc_draw_circle(GraphicsContext *gc, int cx, int cy, int radius, unsigned char color);
void gc_fill_circle(GraphicsContext *gc, int cx, int cy, int radius, unsigned char color);

/* Pattern fill functions */
void gc_fill_rect_pattern(GraphicsContext *gc, int x, int y, int w, int h, Pattern8x8 *pattern);
void gc_fill_rect_current_pattern(GraphicsContext *gc, int x, int y, int w, int h);

/* Utility functions for working with patterns */
void pattern_create_solid(Pattern8x8 *pattern, int fill);  /* All 0s or all 1s */
void pattern_create_checkerboard(Pattern8x8 *pattern);     /* Classic checkerboard */
void pattern_create_horizontal_stripes(Pattern8x8 *pattern, int width);
void pattern_create_vertical_stripes(Pattern8x8 *pattern, int width);
void pattern_create_diagonal(Pattern8x8 *pattern);
void pattern_create_dots(Pattern8x8 *pattern, int spacing);

/* Internal helper functions */
int gc_clip_line(GraphicsContext *gc, int *x0, int *y0, int *x1, int *y1);
int gc_clip_rect(GraphicsContext *gc, int *x, int *y, int *w, int *h);
void gc_apply_translation(GraphicsContext *gc, int *x, int *y);

#endif