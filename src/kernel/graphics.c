#include "graphics.h"
#include "io.h"
#include "serial.h"
#include "memory.h"
#include "timer.h"

/* VGA font is stored in plane 2 at 0xA0000
 * We need to save it before switching to graphics mode
 * Standard VGA stores 256 characters × 32 bytes each = 8KB
 */
#define VGA_FONT_HEIGHT 16
#define VGA_FONT_SIZE (256 * 32)  /* 8KB for 256 chars, 32 bytes each */
static unsigned char *saved_font = NULL;

/* Standard EGA palette - 64 entries (16 colors used in text mode)
 * Format: R,G,B with 6-bit values (0-63)
 * This matches SeaBIOS pal_ega
 */
static unsigned char ega_palette[] = {
    /* Exact copy from SeaBIOS pal_ega - first 16 are standard text mode colors */
    0x00,0x00,0x00, 0x00,0x00,0x2a, 0x00,0x2a,0x00, 0x00,0x2a,0x2a,
    0x2a,0x00,0x00, 0x2a,0x00,0x2a, 0x2a,0x2a,0x00, 0x2a,0x2a,0x2a,
    0x00,0x00,0x15, 0x00,0x00,0x3f, 0x00,0x2a,0x15, 0x00,0x2a,0x3f,
    0x2a,0x00,0x15, 0x2a,0x00,0x3f, 0x2a,0x2a,0x15, 0x2a,0x2a,0x3f,
    0x00,0x15,0x00, 0x00,0x15,0x2a, 0x00,0x3f,0x00, 0x00,0x3f,0x2a,
    0x2a,0x15,0x00, 0x2a,0x15,0x2a, 0x2a,0x3f,0x00, 0x2a,0x3f,0x2a,
    0x00,0x15,0x15, 0x00,0x15,0x3f, 0x00,0x3f,0x15, 0x00,0x3f,0x3f,
    0x2a,0x15,0x15, 0x2a,0x15,0x3f, 0x2a,0x3f,0x15, 0x2a,0x3f,0x3f,
    0x15,0x00,0x00, 0x15,0x00,0x2a, 0x15,0x2a,0x00, 0x15,0x2a,0x2a,
    0x3f,0x00,0x00, 0x3f,0x00,0x2a, 0x3f,0x2a,0x00, 0x3f,0x2a,0x2a,
    0x15,0x00,0x15, 0x15,0x00,0x3f, 0x15,0x2a,0x15, 0x15,0x2a,0x3f,
    0x3f,0x00,0x15, 0x3f,0x00,0x3f, 0x3f,0x2a,0x15, 0x3f,0x2a,0x3f,
    0x15,0x15,0x00, 0x15,0x15,0x2a, 0x15,0x3f,0x00, 0x15,0x3f,0x2a,
    0x3f,0x15,0x00, 0x3f,0x15,0x2a, 0x3f,0x3f,0x00, 0x3f,0x3f,0x2a,
    0x15,0x15,0x15, 0x15,0x15,0x3f, 0x15,0x3f,0x15, 0x15,0x3f,0x3f,
    0x3f,0x15,0x15, 0x3f,0x15,0x3f, 0x3f,0x3f,0x15, 0x3f,0x3f,0x3f
};

/* Aquinas custom palette for graphics mode
 * Emphasizes grays, reds, yellow-golds, and cyans
 * Format: R,G,B with 6-bit values (0-63)
 */
static unsigned char aquinas_palette[] = {
    /* Grayscale foundation (6 shades) */
    0x00, 0x00, 0x00,  /* 0: Black */
    0x10, 0x10, 0x10,  /* 1: Dark gray */
    0x20, 0x20, 0x20,  /* 2: Medium dark gray */
    0x30, 0x30, 0x30,  /* 3: Medium gray */
    0x38, 0x38, 0x38,  /* 4: Light gray */
    0x3F, 0x3F, 0x3F,  /* 5: White */
    
    /* Reds (3 shades) */
    0x20, 0x08, 0x08,  /* 6: Dark red */
    0x30, 0x0C, 0x0C,  /* 7: Medium red */
    0x3F, 0x10, 0x10,  /* 8: Bright red */
    
    /* Yellow-golds (3 shades) */
    0x28, 0x20, 0x08,  /* 9:  Dark gold */
    0x38, 0x30, 0x10,  /* 10: Medium gold */
    0x3F, 0x38, 0x18,  /* 11: Bright yellow-gold */
    
    /* Cyans (3 shades) */
    0x08, 0x20, 0x28,  /* 12: Dark cyan */
    0x10, 0x30, 0x38,  /* 13: Medium cyan */
    0x18, 0x38, 0x3F,  /* 14: Bright cyan */
    
    /* Special purpose */
    0x2C, 0x28, 0x20   /* 15: Warm gray (for text backgrounds) */
};

/* UI Color definitions for Aquinas palette */
#define COLOR_BACKGROUND     3   /* Medium gray */
#define COLOR_TEXT          5   /* White */
#define COLOR_TEXT_DIM      1   /* Dark gray */
#define COLOR_BORDER        4   /* Light gray */
#define COLOR_HIGHLIGHT     14  /* Bright cyan */
#define COLOR_SELECTION     8   /* Bright red */
#define COLOR_CURSOR        11  /* Bright yellow-gold */
#define COLOR_LINK          13  /* Medium cyan */
#define COLOR_COMMAND       10  /* Medium gold */
#define COLOR_STATUS_BAR    2   /* Medium dark gray */
#define COLOR_ACTIVE_PANE   14  /* Bright cyan */
#define COLOR_VIM_VISUAL    8   /* Bright red */

/* Set the Aquinas custom palette for graphics mode */
void set_aquinas_palette(void) {
    int i;
    
    /* First, reset attribute controller flip-flop */
    inb(0x3DA);
    
    /* Set attribute controller palette registers to map straight through
     * This ensures color index N maps to DAC color N */
    for (i = 0; i < 16; i++) {
        outb(0x3C0, i);      /* Select palette register i */
        outb(0x3C0, i);      /* Map to DAC color i */
    }
    
    /* Set the Attribute Mode Control register for graphics */
    outb(0x3C0, 0x10);
    outb(0x3C0, 0x01);  /* Graphics mode, 1 pixel per clock */
    
    /* Enable video display */
    outb(0x3C0, 0x20);
    
    /* Now set the DAC palette with our custom colors */
    for (i = 0; i < 16; i++) {
        outb(0x3C8, i);  /* Select DAC color index */
        outb(0x3C9, aquinas_palette[i * 3]);      /* Red */
        outb(0x3C9, aquinas_palette[i * 3 + 1]);  /* Green */
        outb(0x3C9, aquinas_palette[i * 3 + 2]);  /* Blue */
    }
    
    /* Fill remaining DAC entries (16-255) with black */
    for (i = 16; i < 256; i++) {
        outb(0x3C8, i);
        outb(0x3C9, 0x00);  /* R */
        outb(0x3C9, 0x00);  /* G */
        outb(0x3C9, 0x00);  /* B */
    }
    
    serial_write_string("Set Aquinas custom palette with proper attribute mapping\n");
}

/* Restore the standard EGA/VGA DAC palette */
void restore_dac_palette(void) {
    int i;
    
    /* Select palette index 0 */
    outb(0x3C8, 0x00);
    
    /* Write all 64 palette entries (192 bytes total) */
    for (i = 0; i < sizeof(ega_palette); i++) {
        outb(0x3C9, ega_palette[i]);
    }
    
    /* CRITICAL: Entries 56-63 (0x38-0x3F) contain the bright colors!
     * The attribute controller maps colors 8-15 to DAC 0x38-0x3F,
     * so these must have the correct bright color values.
     * The ega_palette already has these at the right positions. */
    
    /* Fill remaining entries (64-255) with black */
    for (i = 64; i < 256; i++) {
        outb(0x3C8, i);
        outb(0x3C9, 0x00);  /* R */
        outb(0x3C9, 0x00);  /* G */
        outb(0x3C9, 0x00);  /* B */
    }
    
    serial_write_string("Restored DAC palette with proper bright colors at 0x38-0x3F\n");
}

/* Save VGA font from plane 2 */
void save_vga_font(void) {
    unsigned char *vga_mem = (unsigned char *)0xA0000;
    int i;
    
    if (saved_font == NULL) {
        saved_font = (unsigned char *)malloc(VGA_FONT_SIZE);
        if (saved_font == NULL) {
            serial_write_string("Failed to allocate memory for font backup\n");
            return;
        }
    }
    
    /* Set up VGA to read from plane 2 (font data) */
    outb(0x3CE, 0x04);  /* Graphics Controller: Read Map Select Register */
    outb(0x3CF, 0x02);  /* Select plane 2 for reading */
    
    outb(0x3CE, 0x05);  /* Graphics Mode Register */
    outb(0x3CF, 0x00);  /* Write mode 0, read mode 0 */
    
    outb(0x3CE, 0x06);  /* Miscellaneous Register */
    outb(0x3CF, 0x04);  /* Map memory at A0000h, no odd/even */
    
    /* Read font data from VGA memory */
    for (i = 0; i < VGA_FONT_SIZE; i++) {
        saved_font[i] = vga_mem[i];
    }
    
    serial_write_string("Saved VGA font (8KB)\n");
}

/* Restore VGA font to plane 2 */
void restore_vga_font(void) {
    unsigned char *vga_mem = (unsigned char *)0xA0000;
    int i;
    
    if (saved_font == NULL) {
        serial_write_string("No saved font to restore\n");
        return;
    }
    
    /* Set up VGA to write to plane 2 (font data) */
    outb(0x3C4, 0x02);  /* Sequencer: Map Mask Register */
    outb(0x3C5, 0x04);  /* Enable write to plane 2 only */
    
    outb(0x3C4, 0x04);  /* Memory Mode Register */
    outb(0x3C5, 0x06);  /* Disable odd/even, no chain4 */
    
    outb(0x3CE, 0x05);  /* Graphics Mode Register */
    outb(0x3CF, 0x00);  /* Write mode 0 */
    
    outb(0x3CE, 0x06);  /* Miscellaneous Register */
    outb(0x3CF, 0x04);  /* Map memory at A0000h, no odd/even */
    
    /* Write font data back to VGA memory */
    for (i = 0; i < VGA_FONT_SIZE; i++) {
        vga_mem[i] = saved_font[i];
    }
    
    /* Restore normal text mode memory access */
    outb(0x3C4, 0x02);  /* Map Mask Register */
    outb(0x3C5, 0x03);  /* Enable planes 0 and 1 (text/attribute) */
    
    outb(0x3C4, 0x04);  /* Memory Mode Register */
    outb(0x3C5, 0x02);  /* Odd/even mode */
    
    outb(0x3CE, 0x05);  /* Graphics Mode Register */
    outb(0x3CF, 0x10);  /* Odd/even mode */
    
    outb(0x3CE, 0x06);  /* Miscellaneous Register */
    outb(0x3CF, 0x0E);  /* Map at B8000h for text mode */
    
    serial_write_string("Restored VGA font\n");
}

void set_mode_12h(void) {
    unsigned char seq_data[] = {
        0x03, 0x01, 0x0F, 0x00, 0x06
    };
    
    unsigned char crtc_data[] = {
        0x5F, 0x4F, 0x50, 0x82, 0x54, 0x80, 0x0B, 0x3E,
        0x00, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0xEA, 0x0C, 0xDF, 0x28, 0x00, 0xE7, 0x04, 0xE3,
        0xFF
    };
    
    unsigned char graphics_data[] = {
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x05, 0x0F,
        0xFF
    };
    
    unsigned char attr_data[] = {
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x14, 0x07,
        0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F,
        0x01, 0x00, 0x0F, 0x00, 0x00
    };
    
    int i;
    unsigned char tmp;
    
    serial_write_string("Switching to graphics mode 0x12...\n");
    
    /* Set misc output register for graphics mode */
    outb(0x3C2, 0xE3);
    
    /* Reset attribute controller flip-flop */
    inb(0x3DA);
    /* Disable display during mode switch */
    outb(0x3C0, 0x00);
    
    /* Program sequencer */
    for (i = 0; i < 5; i++) {
        outb(0x3C4, i);
        outb(0x3C5, seq_data[i]);
    }
    
    /* Unlock CRTC registers */
    outb(0x3D4, 0x11);
    tmp = inb(0x3D5);
    outb(0x3D5, tmp & 0x7F);
    
    /* Program CRTC */
    for (i = 0; i < 25; i++) {
        outb(0x3D4, i);
        outb(0x3D5, crtc_data[i]);
    }
    
    /* Program graphics controller */
    for (i = 0; i < 9; i++) {
        outb(0x3CE, i);
        outb(0x3CF, graphics_data[i]);
    }
    
    /* Program attribute controller */
    inb(0x3DA);
    for (i = 0; i < 21; i++) {
        outb(0x3C0, i);
        outb(0x3C0, attr_data[i]);
    }
    
    /* Re-enable display */
    outb(0x3C0, 0x20);
    
    serial_write_string("Graphics mode 0x12 set\n");
}

void set_mode_03h(void) {
    int i;
    unsigned char tmp;
    /* CRTC registers for 80x25 text mode - matching SeaBIOS crtc_03 */
    unsigned char crtc_vals[] = {
        /* 0x00-0x07 */
        0x5F, 0x4F, 0x50, 0x82, 0x55, 0x81, 0xBF, 0x1F,
        /* 0x08-0x0F - Note: 0x09 = 0x4F for 16-line chars, 0x0A/0x0B for cursor */
        0x00, 0x4F, 0x0D, 0x0E, 0x00, 0x00, 0x00, 0x50,
        /* 0x10-0x18 */
        0x9C, 0x0E, 0x8F, 0x28, 0x1F, 0x96, 0xB9, 0xA3,
        0xFF
    };
    /* Attribute controller palette mapping - matches SeaBIOS actl_01 */
    unsigned char attr_palette[] = {
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x14, 0x07,  /* 0-7: Note 6→0x14 */
        0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F   /* 8-15: Map to 0x38-0x3F */
    };
    
    serial_write_string("Switching back to text mode 0x03...\n");
    
    /* Miscellaneous Output Register first - set before other registers */
    outb(0x3C2, 0x67);  /* 0x67 for 80x25 color text mode */
    
    /* Sequencer reset and programming */
    outb(0x3C4, 0x00);
    outb(0x3C5, 0x01);  /* Assert synchronous reset */
    
    /* Program sequencer - matching SeaBIOS sequ_03 */
    outb(0x3C4, 0x01); outb(0x3C5, 0x00);  /* 0x00 = 9-dot characters enabled! */
    outb(0x3C4, 0x02); outb(0x3C5, 0x03);  /* Map mask - planes 0 and 1 for text */
    outb(0x3C4, 0x03); outb(0x3C5, 0x00);  /* Character map select */
    outb(0x3C4, 0x04); outb(0x3C5, 0x02);  /* Memory mode - odd/even addressing */
    
    /* Release sequencer reset */
    outb(0x3C4, 0x00);
    outb(0x3C5, 0x03);  /* De-assert reset - both reset bits cleared */
    
    /* Unlock CRTC */
    outb(0x3D4, 0x11);
    tmp = inb(0x3D5);
    outb(0x3D5, tmp & 0x7F);
    
    for (i = 0; i < 25; i++) {
        outb(0x3D4, i);
        outb(0x3D5, crtc_vals[i]);
    }
    
    /* Graphics Controller - matching SeaBIOS values for text mode */
    outb(0x3CE, 0x00); outb(0x3CF, 0x00);  /* Set/Reset */
    outb(0x3CE, 0x01); outb(0x3CF, 0x00);  /* Enable Set/Reset */
    outb(0x3CE, 0x02); outb(0x3CF, 0x00);  /* Color Compare */
    outb(0x3CE, 0x03); outb(0x3CF, 0x00);  /* Data Rotate */
    outb(0x3CE, 0x04); outb(0x3CF, 0x00);  /* Read Map Select */
    outb(0x3CE, 0x05); outb(0x3CF, 0x10);  /* Graphics Mode - 0x10 for text (host odd/even) */
    outb(0x3CE, 0x06); outb(0x3CF, 0x0E);  /* Misc - 0x0E (B8000 map, chain odd/even) */
    outb(0x3CE, 0x07); outb(0x3CF, 0x00);  /* Color Don't Care */
    outb(0x3CE, 0x08); outb(0x3CF, 0xFF);  /* Bit Mask */
    
    /* Attribute Controller */
    inb(0x3DA);  /* Reset flip-flop */
    
    /* Palette registers - CRITICAL: Must match SeaBIOS actl_01 mapping! */
    /* This maps text colors to DAC palette indices */
    for (i = 0; i < 16; i++) {
        outb(0x3C0, i);
        outb(0x3C0, attr_palette[i]);
    }
    
    /* Mode Control - matching SeaBIOS actl_01 */
    outb(0x3C0, 0x10); 
    outb(0x3C0, 0x0C);  /* Bit 3=blink, bit 2=9th dot handling, bit 0=text mode */
    
    outb(0x3C0, 0x11); outb(0x3C0, 0x00);  /* Overscan color */
    outb(0x3C0, 0x12); outb(0x3C0, 0x0F);  /* Color plane enable */
    outb(0x3C0, 0x13); outb(0x3C0, 0x00);  /* Horizontal pixel panning */
    outb(0x3C0, 0x14); outb(0x3C0, 0x00);  /* Color select */
    
    /* Enable display */
    outb(0x3C0, 0x20);
    
    serial_write_string("Text mode 0x03 restored\n");
}

void set_pixel(int x, int y, unsigned char color) {
    unsigned char *vga = (unsigned char *)VGA_GRAPHICS_BUFFER;
    unsigned int offset;
    unsigned char plane;
    
    if (x < 0 || x >= VGA_WIDTH_12H || y < 0 || y >= VGA_HEIGHT_12H) {
        return;
    }
    
    offset = (y * (VGA_WIDTH_12H / 8)) + (x / 8);
    plane = 0x80 >> (x & 7);
    
    outb(0x3C4, 0x02);
    outb(0x3C5, 0x0F);
    
    outb(0x3CE, 0x08);
    outb(0x3CF, plane);
    
    vga[offset] &= ~plane;
    vga[offset] |= (color ? plane : 0);
}

void draw_rectangle(int x, int y, int width, int height, unsigned char color) {
    unsigned char *vga = (unsigned char *)VGA_GRAPHICS_BUFFER;
    int row, col;
    int x1, x2, y1, y2;
    int start_byte, end_byte;
    unsigned int offset;
    unsigned char mask;
    volatile unsigned char dummy;  /* For latch operations */
    
    if (x >= VGA_WIDTH_12H || y >= VGA_HEIGHT_12H) return;
    if (width <= 0 || height <= 0) return;
    
    x1 = x;
    y1 = y;
    x2 = x + width - 1;
    y2 = y + height - 1;
    
    /* Clip to screen bounds */
    if (x1 < 0) x1 = 0;
    if (y1 < 0) y1 = 0;
    if (x2 >= VGA_WIDTH_12H) x2 = VGA_WIDTH_12H - 1;
    if (y2 >= VGA_HEIGHT_12H) y2 = VGA_HEIGHT_12H - 1;
    
    /* Set up VGA for efficient filled rectangle drawing */
    
    /* Set Graphics Mode to Write Mode 0 */
    outb(0x3CE, 0x05);  /* Graphics Mode Register */
    outb(0x3CF, 0x00);  /* Write mode 0 */
    
    /* Enable all planes for writing */
    outb(0x3C4, 0x02);  /* Map Mask Register */
    outb(0x3C5, 0x0F);  /* Enable all 4 planes */
    
    /* Set the color in the Set/Reset register */
    outb(0x3CE, 0x00);  /* Set/Reset Register */
    outb(0x3CF, color); /* Color value to be used for all planes */
    
    /* Enable Set/Reset for all planes */
    outb(0x3CE, 0x01);  /* Enable Set/Reset Register */
    outb(0x3CF, 0x0F);  /* Enable Set/Reset for all 4 planes */
    
    /* Draw the rectangle row by row */
    for (row = y1; row <= y2; row++) {
        start_byte = x1 / 8;
        end_byte = x2 / 8;
        offset = row * (VGA_WIDTH_12H / 8);
        
        if (start_byte == end_byte) {
            /* Rectangle fits within a single byte */
            mask = (0xFF >> (x1 & 7)) & (0xFF << (7 - (x2 & 7)));
            outb(0x3CE, 0x08);  /* Bit Mask Register */
            outb(0x3CF, mask);
            /* Read to latch, then write (value doesn't matter with Set/Reset) */
            dummy = vga[offset + start_byte];
            vga[offset + start_byte] = 0xFF;
        } else {
            /* First byte (partial) */
            if (x1 & 7) {  /* Only if not byte-aligned */
                mask = 0xFF >> (x1 & 7);
                outb(0x3CE, 0x08);  /* Bit Mask Register */
                outb(0x3CF, mask);
                dummy = vga[offset + start_byte];
                vga[offset + start_byte] = 0xFF;
                start_byte++;
            }
            
            /* Middle bytes (full bytes) */
            if (start_byte <= end_byte) {
                outb(0x3CE, 0x08);  /* Bit Mask Register */
                outb(0x3CF, 0xFF);  /* All pixels in byte */
                for (col = start_byte; col < end_byte; col++) {
                    vga[offset + col] = 0xFF;  /* Value doesn't matter with Set/Reset */
                }
            }
            
            /* Last byte (partial) */
            if ((x2 & 7) != 7) {  /* Only if not byte-aligned */
                mask = 0xFF << (7 - (x2 & 7));
                outb(0x3CE, 0x08);  /* Bit Mask Register */
                outb(0x3CF, mask);
                dummy = vga[offset + end_byte];
                vga[offset + end_byte] = 0xFF;
            } else if (start_byte <= end_byte) {
                /* Last byte is full */
                vga[offset + end_byte] = 0xFF;
            }
        }
    }
    
    /* Restore default state */
    outb(0x3CE, 0x01);  /* Enable Set/Reset Register */
    outb(0x3CF, 0x00);  /* Disable Set/Reset */
    outb(0x3CE, 0x08);  /* Bit Mask Register */
    outb(0x3CF, 0xFF);  /* Enable all bits */
}

void clear_graphics_screen(unsigned char color) {
    unsigned char *vga = (unsigned char *)VGA_GRAPHICS_BUFFER;
    int plane, i;
    unsigned char fill_value;
    
    for (plane = 0; plane < 4; plane++) {
        outb(0x3C4, 0x02);
        outb(0x3C5, 1 << plane);
        
        fill_value = (color & (1 << plane)) ? 0xFF : 0x00;
        
        for (i = 0; i < (VGA_WIDTH_12H * VGA_HEIGHT_12H) / 8; i++) {
            vga[i] = fill_value;
        }
    }
}

void graphics_demo(void) {
    int running = 1;
    int animation_frame = 0;
    unsigned int last_frame_time;
    unsigned int current_time;
    int x_pos = 0;
    int y_pos = 0;
    int prev_x_pos = 0;  /* Track previous position for proper clearing */
    int prev_y_pos = 0;
    int color = 1;
    int i;
    
    /* Frame timing constants */
    #define FRAME_DELAY_MS 50  /* 50ms = 20 FPS */
    
    /* Save font before switching to graphics */
    save_vga_font();
    
    set_mode_12h();
    
    /* Set our custom Aquinas palette */
    set_aquinas_palette();
    
    /* Clear screen with medium gray background */
    clear_graphics_screen(COLOR_BACKGROUND);
    
    /* Draw UI demo with the new palette */
    /* Grayscale showcase */
    draw_rectangle(20, 20, 60, 60, 0);   /* Black */
    draw_rectangle(90, 20, 60, 60, 1);   /* Dark gray */
    draw_rectangle(160, 20, 60, 60, 2);  /* Medium dark gray */
    draw_rectangle(230, 20, 60, 60, 3);  /* Medium gray */
    draw_rectangle(300, 20, 60, 60, 4);  /* Light gray */
    draw_rectangle(370, 20, 60, 60, 5);  /* White */
    
    /* Red showcase */
    draw_rectangle(20, 100, 100, 50, 6);   /* Dark red */
    draw_rectangle(130, 100, 100, 50, 7);  /* Medium red */
    draw_rectangle(240, 100, 100, 50, 8);  /* Bright red */
    
    /* Gold showcase */
    draw_rectangle(20, 170, 100, 50, 9);   /* Dark gold */
    draw_rectangle(130, 170, 100, 50, 10); /* Medium gold */
    draw_rectangle(240, 170, 100, 50, 11); /* Bright yellow-gold */
    
    /* Cyan showcase */
    draw_rectangle(20, 240, 100, 50, 12);  /* Dark cyan */
    draw_rectangle(130, 240, 100, 50, 13); /* Medium cyan */
    draw_rectangle(240, 240, 100, 50, 14); /* Bright cyan */
    
    /* UI element demos */
    draw_rectangle(10, 320, 620, 30, COLOR_STATUS_BAR);    /* Status bar */
    draw_rectangle(15, 325, 100, 20, COLOR_COMMAND);       /* Command button */
    draw_rectangle(120, 325, 100, 20, COLOR_LINK);         /* Link button */
    draw_rectangle(225, 325, 100, 20, COLOR_HIGHLIGHT);    /* Highlighted item */
    draw_rectangle(330, 325, 100, 20, COLOR_SELECTION);    /* Selected item */
    
    /* Border demo */
    draw_rectangle(450, 100, 150, 100, COLOR_BORDER);
    draw_rectangle(455, 105, 140, 90, COLOR_BACKGROUND);
    
    /* Initialize timing */
    last_frame_time = get_ticks();
    
    while (running) {
        /* Check for ESC key to exit */
        if (inb(0x60) == 0x01) {
            running = 0;
        }
        
        /* Get current time */
        current_time = get_ticks();
        
        /* Update animation only if enough time has passed */
        if (current_time - last_frame_time >= FRAME_DELAY_MS) {
            /* Clear previous animated rectangle using PREVIOUS position */
            if (animation_frame > 0) {  /* Don't clear on first frame */
                draw_rectangle(380 + prev_x_pos, 240 + prev_y_pos, 60, 40, COLOR_BACKGROUND);
            }
            
            /* Save current position as previous for next frame */
            prev_x_pos = x_pos;
            prev_y_pos = y_pos;
            
            /* Update position and color */
            animation_frame++;
            x_pos = (animation_frame * 2) % 40;  /* Move 2 pixels per frame */
            y_pos = (animation_frame) % 30;      /* Move 1 pixel per frame vertically */
            
            /* Cycle through cyan and gold colors for animation */
            if ((animation_frame / 10) % 4 == 0) {
                color = COLOR_CURSOR;       /* Bright yellow-gold */
            } else if ((animation_frame / 10) % 4 == 1) {
                color = COLOR_HIGHLIGHT;    /* Bright cyan */
            } else if ((animation_frame / 10) % 4 == 2) {
                color = COLOR_COMMAND;      /* Medium gold */
            } else {
                color = COLOR_LINK;         /* Medium cyan */
            }
            
            /* Draw new animated rectangle */
            draw_rectangle(380 + x_pos, 240 + y_pos, 60, 40, color);
            
            /* Update last frame time */
            last_frame_time = current_time;
        }
    }
    
    set_mode_03h();
    
    /* Restore font after switching back to text mode */
    restore_vga_font();
    
    /* Restore DAC palette for proper colors */
    restore_dac_palette();
}