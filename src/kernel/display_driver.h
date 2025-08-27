#ifndef DISPLAY_DRIVER_H
#define DISPLAY_DRIVER_H

/* Display driver interface - abstraction layer for different display devices
 * Supports both VGA mode 12h and DISPI/VBE implementations
 */

typedef struct DisplayDriver {
    /* Display properties */
    int width;
    int height;
    int bpp;  /* Bits per pixel - we'll use 8 for palette mode */
    
    /* Driver lifecycle */
    void (*init)(void);
    void (*shutdown)(void);
    
    /* Basic pixel operations */
    void (*set_pixel)(int x, int y, unsigned char color);
    unsigned char (*get_pixel)(int x, int y);
    
    /* Optimized rectangle operations */
    void (*fill_rect)(int x, int y, int w, int h, unsigned char color);
    void (*blit)(int x, int y, int w, int h, unsigned char *src, int src_stride);
    
    /* Palette management - 16 colors with RGB components */
    void (*set_palette)(unsigned char palette[16][3]);
    void (*get_palette)(unsigned char palette[16][3]);
    
    /* Optional: Hardware-specific optimizations */
    void (*clear_screen)(unsigned char color);
    void (*vsync)(void);  /* Wait for vertical sync */
    
    /* Driver name for debugging */
    const char *name;
} DisplayDriver;

/* Global active driver */
extern DisplayDriver *active_display_driver;

/* Driver initialization functions */
void display_set_driver(DisplayDriver *driver);
DisplayDriver *display_get_driver(void);

/* Common display operations that use the active driver */
void display_init(void);
void display_shutdown(void);
void display_set_pixel(int x, int y, unsigned char color);
unsigned char display_get_pixel(int x, int y);
void display_fill_rect(int x, int y, int w, int h, unsigned char color);
void display_blit(int x, int y, int w, int h, unsigned char *src, int src_stride);
void display_clear(unsigned char color);

#endif