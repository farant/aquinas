/* DISPI (Display Interface) implementation for Bochs/QEMU VGA
 *
 * This driver provides access to the linear framebuffer mode of the Bochs VGA
 * adapter, which is emulated by QEMU. It allows us to use 640x480 resolution
 * with 8-bit indexed color (256 colors, though we only use 16).
 *
 * The DISPI interface is much simpler than full VBE - it's just a set of
 * I/O ports that control the display mode and provide the framebuffer address.
 */

#include "dispi.h"
#include "display_driver.h"
#include "io.h"
#include "serial.h"
#include "pci.h"
#include "memory.h"

/* Framebuffer information */
static unsigned char* framebuffer = (unsigned char*)DISPI_LFB_PHYSICAL_ADDRESS;
static unsigned int framebuffer_size = 0;
static int dispi_available = 0;

/* Double buffering support */
static unsigned char* backbuffer = NULL;
static int double_buffered = 0;

/* Write to DISPI register */
void dispi_write(unsigned short index, unsigned short value) {
    port_word_out(VBE_DISPI_IOPORT_INDEX, index);
    port_word_out(VBE_DISPI_IOPORT_DATA, value);
}

/* Read from DISPI register */
unsigned short dispi_read(unsigned short index) {
    port_word_out(VBE_DISPI_IOPORT_INDEX, index);
    return port_word_in(VBE_DISPI_IOPORT_DATA);
}

/* Detect if DISPI is available */
int dispi_detect(void) {
    /* Try writing ID0 and reading it back */
    dispi_write(VBE_DISPI_INDEX_ID, VBE_DISPI_ID0);
    if (dispi_read(VBE_DISPI_INDEX_ID) != VBE_DISPI_ID0) {
        return 0;
    }
    
    /* Now set to ID5 for full feature set */
    dispi_write(VBE_DISPI_INDEX_ID, VBE_DISPI_ID5);
    if (dispi_read(VBE_DISPI_INDEX_ID) < VBE_DISPI_ID5) {
        /* Fall back to ID4 if ID5 not supported */
        dispi_write(VBE_DISPI_INDEX_ID, VBE_DISPI_ID4);
    }
    
    serial_write_string("DISPI detected, version: ");
    serial_write_hex(dispi_read(VBE_DISPI_INDEX_ID));
    serial_write_string("\n");
    
    return 1;
}

/* Initialize DISPI */
void dispi_init(void) {
    unsigned short xres, yres, bpp;
    unsigned int fb_addr;
    
    if (!dispi_detect()) {
        serial_write_string("ERROR: DISPI not available\n");
        dispi_available = 0;
        return;
    }
    
    dispi_available = 1;
    
    /* Detect framebuffer address from PCI */
    fb_addr = pci_find_vga_framebuffer();
    if (fb_addr != 0) {
        framebuffer = (unsigned char*)fb_addr;
        serial_write_string("Using detected framebuffer at: ");
        serial_write_hex(fb_addr);
        serial_write_string("\n");
    } else {
        serial_write_string("PCI detection failed, using default framebuffer\n");
    }
    
    /* Set our desired mode */
    dispi_set_mode(DISPI_WIDTH, DISPI_HEIGHT, DISPI_BPP);
    
    /* Read back what was actually set */
    xres = dispi_read(VBE_DISPI_INDEX_XRES);
    yres = dispi_read(VBE_DISPI_INDEX_YRES);
    bpp = dispi_read(VBE_DISPI_INDEX_BPP);
    
    /* Calculate framebuffer size */
    framebuffer_size = DISPI_WIDTH * DISPI_HEIGHT * (DISPI_BPP / 8);
    
    serial_write_string("DISPI initialized: ");
    serial_write_hex(DISPI_WIDTH);
    serial_write_string("x");
    serial_write_hex(DISPI_HEIGHT);
    serial_write_string("x");
    serial_write_hex(DISPI_BPP);
    serial_write_string(" FB at ");
    serial_write_hex((unsigned int)framebuffer);
    serial_write_string("\n");
    
    serial_write_string("DISPI actual mode: ");
    serial_write_hex(xres);
    serial_write_string("x");
    serial_write_hex(yres);
    serial_write_string("x");
    serial_write_hex(bpp);
    serial_write_string("\n");
}

/* Set display mode */
void dispi_set_mode(int width, int height, int bpp) {
    unsigned short enable_val;
    
    /* Disable display first */
    dispi_disable();
    
    /* Clear screen by not setting NOCLEARMEM */
    
    /* Set resolution and color depth */
    dispi_write(VBE_DISPI_INDEX_XRES, width);
    dispi_write(VBE_DISPI_INDEX_YRES, height);
    dispi_write(VBE_DISPI_INDEX_BPP, bpp);
    
    /* Set virtual resolution same as physical (no scrolling) */
    dispi_write(VBE_DISPI_INDEX_VIRT_WIDTH, width);
    dispi_write(VBE_DISPI_INDEX_VIRT_HEIGHT, height);
    
    /* No offset */
    dispi_write(VBE_DISPI_INDEX_X_OFFSET, 0);
    dispi_write(VBE_DISPI_INDEX_Y_OFFSET, 0);
    
    /* Enable with linear framebuffer */
    dispi_enable(1);
    
    /* Read back enable value to verify */
    enable_val = dispi_read(VBE_DISPI_INDEX_ENABLE);
    serial_write_string("DISPI enable register: ");
    serial_write_hex(enable_val);
    serial_write_string("\n");
}

/* Enable DISPI */
void dispi_enable(int lfb_enable) {
    unsigned short flags = VBE_DISPI_ENABLED;
    
    if (lfb_enable) {
        flags |= VBE_DISPI_LFB_ENABLED;
    }
    
    /* Let DISPI clear the framebuffer on mode switch */
    /* flags |= VBE_DISPI_NOCLEARMEM; */
    
    dispi_write(VBE_DISPI_INDEX_ENABLE, flags);
}

/* Disable DISPI */
void dispi_disable(void) {
    dispi_write(VBE_DISPI_INDEX_ENABLE, VBE_DISPI_DISABLED);
}

/* Get framebuffer address */
unsigned char* dispi_get_framebuffer(void) {
    return framebuffer;
}

/* Get framebuffer size */
unsigned int dispi_get_framebuffer_size(void) {
    return framebuffer_size;
}

/* ============================================================================
 * Display Driver Implementation
 * ============================================================================ */

/* Driver functions */
static void dispi_driver_init(void);
static void dispi_driver_shutdown(void);
static void dispi_driver_set_pixel(int x, int y, unsigned char color);
static unsigned char dispi_driver_get_pixel(int x, int y);
static void dispi_driver_fill_rect(int x, int y, int w, int h, unsigned char color);
static void dispi_driver_blit(int x, int y, int w, int h, unsigned char *src, int src_stride);
static void dispi_driver_set_palette(unsigned char palette[16][3]);
static void dispi_driver_get_palette(unsigned char palette[16][3]);
static void dispi_driver_clear_screen(unsigned char color);
static void dispi_driver_vsync(void);

/* The DISPI display driver */
static DisplayDriver dispi_driver = {
    DISPI_WIDTH,     /* width */
    DISPI_HEIGHT,    /* height */
    DISPI_BPP,       /* bpp */
    
    dispi_driver_init,        /* init */
    dispi_driver_shutdown,     /* shutdown */
    
    dispi_driver_set_pixel,    /* set_pixel */
    dispi_driver_get_pixel,    /* get_pixel */
    
    dispi_driver_fill_rect,    /* fill_rect */
    dispi_driver_blit,         /* blit */
    
    dispi_driver_set_palette,  /* set_palette */
    dispi_driver_get_palette,  /* get_palette */
    
    dispi_driver_clear_screen, /* clear_screen */
    dispi_driver_vsync,        /* vsync */
    
    "DISPI/VBE"               /* name */
};

/* Initialize driver */
static void dispi_driver_init(void) {
    dispi_init();
}

/* Shutdown driver */
static void dispi_driver_shutdown(void) {
    dispi_disable();
}

/* Set a pixel */
static void dispi_driver_set_pixel(int x, int y, unsigned char color) {
    unsigned char* target = double_buffered ? backbuffer : framebuffer;
    if (x >= 0 && x < DISPI_WIDTH && y >= 0 && y < DISPI_HEIGHT) {
        target[y * DISPI_WIDTH + x] = color;
    }
}

/* Get a pixel */
static unsigned char dispi_driver_get_pixel(int x, int y) {
    unsigned char* source = double_buffered ? backbuffer : framebuffer;
    if (x >= 0 && x < DISPI_WIDTH && y >= 0 && y < DISPI_HEIGHT) {
        return source[y * DISPI_WIDTH + x];
    }
    return 0;
}

/* Fill a rectangle */
static void dispi_driver_fill_rect(int x, int y, int w, int h, unsigned char color) {
    unsigned char* target;
    unsigned char* fb;
    int row, col;
    
    /* Clip to screen bounds */
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > DISPI_WIDTH) { w = DISPI_WIDTH - x; }
    if (y + h > DISPI_HEIGHT) { h = DISPI_HEIGHT - y; }
    
    if (w <= 0 || h <= 0) return;
    
    /* Fill the rectangle */
    target = double_buffered ? backbuffer : framebuffer;
    fb = target + y * DISPI_WIDTH + x;
    for (row = 0; row < h; row++) {
        for (col = 0; col < w; col++) {
            fb[col] = color;
        }
        fb += DISPI_WIDTH;
    }
}

/* Blit a buffer to screen */
static void dispi_driver_blit(int x, int y, int w, int h, unsigned char *src, int src_stride) {
    unsigned char* target;
    unsigned char* fb;
    int row, col;
    
    /* Clip to screen bounds */
    if (x < 0) { src -= x; w += x; x = 0; }
    if (y < 0) { src -= y * src_stride; h += y; y = 0; }
    if (x + w > DISPI_WIDTH) { w = DISPI_WIDTH - x; }
    if (y + h > DISPI_HEIGHT) { h = DISPI_HEIGHT - y; }
    
    if (w <= 0 || h <= 0) return;
    
    /* Copy the buffer */
    target = double_buffered ? backbuffer : framebuffer;
    fb = target + y * DISPI_WIDTH + x;
    for (row = 0; row < h; row++) {
        for (col = 0; col < w; col++) {
            fb[col] = src[col];
        }
        src += src_stride;
        fb += DISPI_WIDTH;
    }
}

/* Set palette using VGA DAC registers */
static void dispi_driver_set_palette(unsigned char palette[16][3]) {
    int i;
    /* DISPI uses standard VGA DAC for palette in 8bpp mode */
    for (i = 0; i < 16; i++) {
        port_byte_out(0x3C8, i);  /* DAC write index */
        port_byte_out(0x3C9, palette[i][0] >> 2);  /* Red (6-bit) */
        port_byte_out(0x3C9, palette[i][1] >> 2);  /* Green (6-bit) */
        port_byte_out(0x3C9, palette[i][2] >> 2);  /* Blue (6-bit) */
    }
}

/* Get palette from VGA DAC registers */
static void dispi_driver_get_palette(unsigned char palette[16][3]) {
    int i;
    for (i = 0; i < 16; i++) {
        port_byte_out(0x3C7, i);  /* DAC read index */
        palette[i][0] = port_byte_in(0x3C9) << 2;  /* Red */
        palette[i][1] = port_byte_in(0x3C9) << 2;  /* Green */
        palette[i][2] = port_byte_in(0x3C9) << 2;  /* Blue */
    }
}

/* Clear the entire screen */
static void dispi_driver_clear_screen(unsigned char color) {
    /* Fast clear using memset-like operation */
    unsigned char* target = double_buffered ? backbuffer : framebuffer;
    unsigned int size = DISPI_WIDTH * DISPI_HEIGHT;
    unsigned int i;
    
    /* Fill screen with color */
    for (i = 0; i < size; i++) {
        target[i] = color;
    }
}

/* Wait for vsync (stub - DISPI doesn't provide vsync) */
static void dispi_driver_vsync(void) {
    /* DISPI doesn't have a vsync register, so this is a no-op
     * In a real implementation, we might use a timer or VGA port 0x3DA
     */
}

/* Get the DISPI driver */
DisplayDriver* dispi_get_driver(void) {
    return &dispi_driver;
}

/* Initialize double buffering */
int dispi_init_double_buffer(void) {
    if (!dispi_available) {
        serial_write_string("ERROR: Cannot init double buffer - DISPI not available\n");
        return 0;
    }
    
    if (double_buffered) {
        serial_write_string("Double buffering already initialized\n");
        return 1;
    }
    
    /* Allocate backbuffer */
    backbuffer = (unsigned char*)malloc(framebuffer_size);
    if (!backbuffer) {
        serial_write_string("ERROR: Failed to allocate backbuffer\n");
        return 0;
    }
    
    /* Clear the backbuffer */
    memset(backbuffer, 0, framebuffer_size);
    
    double_buffered = 1;
    serial_write_string("Double buffering initialized with ");
    serial_write_hex(framebuffer_size);
    serial_write_string(" byte backbuffer\n");
    
    return 1;
}

/* Flip buffers - copy backbuffer to framebuffer */
void dispi_flip_buffers(void) {
    if (!double_buffered || !backbuffer) {
        return;
    }
    
    /* Copy backbuffer to framebuffer
     * This is where we'd ideally use hardware page flipping,
     * but DISPI doesn't support multiple framebuffers */
    memcpy(framebuffer, backbuffer, framebuffer_size);
}

/* Get backbuffer for drawing */
unsigned char* dispi_get_backbuffer(void) {
    if (!double_buffered) {
        /* If not double buffered, return framebuffer directly */
        return framebuffer;
    }
    return backbuffer;
}

/* Cleanup double buffering */
void dispi_cleanup_double_buffer(void) {
    if (backbuffer) {
        /* Note: free() is a no-op in our bump allocator,
         * but we clear the pointer for safety */
        free(backbuffer);
        backbuffer = NULL;
    }
    double_buffered = 0;
}

/* Check if double buffering is active */
int dispi_is_double_buffered(void) {
    return double_buffered;
}