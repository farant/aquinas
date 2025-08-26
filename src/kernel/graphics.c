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

/* Text spacing constants */
#define CHAR_WIDTH_TIGHT    8   /* No gap - characters touch */
#define CHAR_WIDTH_NORMAL   9   /* 1 pixel gap - matches text mode */
#define CHAR_WIDTH_LOOSE    10  /* 2 pixel gap - more readable */
#define CHAR_HEIGHT         16  /* VGA font height */
#define LINE_SPACING        18  /* Add 2 pixels between lines */

/* Special value for transparent background */
#define COLOR_TRANSPARENT   0xFF

/* Text metrics structure */
typedef struct {
    int char_width;   /* Including spacing */
    int char_height;
    int line_height;  /* Including line spacing */
} FontMetrics;

/* Default font metrics matching text mode */
static FontMetrics default_font = {9, 16, 18};

/* Mouse cursor definitions */
#define CURSOR_WIDTH 12
#define CURSOR_HEIGHT 20
#define CURSOR_HOTSPOT_X 0
#define CURSOR_HOTSPOT_Y 0

/* Classic arrow cursor bitmap - 12x20 pixels
 * Each row is 2 bytes (12 bits used, 4 padding)
 * 1 = draw pixel, 0 = transparent */
static const unsigned char cursor_arrow[] = {
    0x80, 0x00,  /* X........... */
    0xC0, 0x00,  /* XX.......... */
    0xE0, 0x00,  /* XXX......... */
    0xF0, 0x00,  /* XXXX........ */
    0xF8, 0x00,  /* XXXXX....... */
    0xFC, 0x00,  /* XXXXXX...... */
    0xFE, 0x00,  /* XXXXXXX..... */
    0xFF, 0x00,  /* XXXXXXXX.... */
    0xFF, 0x80,  /* XXXXXXXXX... */
    0xFF, 0xC0,  /* XXXXXXXXXX.. */
    0xFC, 0x00,  /* XXXXXX...... */
    0xEE, 0x00,  /* XXX.XXX..... */
    0xE7, 0x00,  /* XXX..XXX.... */
    0xC3, 0x00,  /* XX....XX.... */
    0xC3, 0x80,  /* XX....XXX... */
    0x01, 0x80,  /* .......XX... */
    0x01, 0x80,  /* .......XX... */
    0x00, 0xC0,  /* ........XX.. */
    0x00, 0xC0,  /* ........XX.. */
    0x00, 0x00,  /* ............ */
};

/* Maximum cursor size for background buffer */
#define MAX_CURSOR_SIZE (32 * 32)

/* Cursor state structure */
typedef struct {
    int x, y;                    /* Current position */
    int visible;                 /* Is cursor shown? */
    const unsigned char *bitmap; /* Current cursor shape */
    int width, height;
    int hotspot_x, hotspot_y;   /* Click point offset */
    unsigned char saved_background[MAX_CURSOR_SIZE];  /* Saved background pixels */
    int saved_x, saved_y;        /* Position where background was saved */
} MouseCursor;

/* Global mouse cursor state */
static MouseCursor mouse_cursor = {
    320,                    /* x */
    240,                    /* y */
    0,                      /* visible - start hidden */
    cursor_arrow,           /* bitmap */
    CURSOR_WIDTH,           /* width */
    CURSOR_HEIGHT,          /* height */
    CURSOR_HOTSPOT_X,       /* hotspot_x */
    CURSOR_HOTSPOT_Y,       /* hotspot_y */
    {0},                    /* saved_background - initialized to 0 */
    -1,                     /* saved_x - invalid initially */
    -1                      /* saved_y - invalid initially */
};

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

/* Read a pixel value from VGA memory */
unsigned char read_pixel(int x, int y) {
    unsigned char *vga = (unsigned char *)VGA_GRAPHICS_BUFFER;
    unsigned int offset;
    unsigned char mask;
    unsigned char color = 0;
    int plane;
    
    if (x < 0 || x >= VGA_WIDTH_12H || y < 0 || y >= VGA_HEIGHT_12H) {
        return 0;
    }
    
    offset = (y * (VGA_WIDTH_12H / 8)) + (x / 8);
    mask = 0x80 >> (x & 7);
    
    /* Read from each plane */
    for (plane = 0; plane < 4; plane++) {
        outb(0x3CE, 0x04);  /* Graphics Controller: Read Map Select */
        outb(0x3CF, plane); /* Select plane to read */
        
        if (vga[offset] & mask) {
            color |= (1 << plane);
        }
    }
    
    return color;
}

void set_pixel(int x, int y, unsigned char color) {
    unsigned char *vga = (unsigned char *)VGA_GRAPHICS_BUFFER;
    unsigned int offset;
    unsigned char mask;
    volatile unsigned char dummy;
    
    if (x < 0 || x >= VGA_WIDTH_12H || y < 0 || y >= VGA_HEIGHT_12H) {
        return;
    }
    
    offset = (y * (VGA_WIDTH_12H / 8)) + (x / 8);
    mask = 0x80 >> (x & 7);  /* Single pixel bit mask */
    
    /* Use Set/Reset for consistency with draw_rectangle */
    
    /* Set Graphics Mode to Write Mode 0 */
    outb(0x3CE, 0x05);
    outb(0x3CF, 0x00);
    
    /* Enable all planes for writing */
    outb(0x3C4, 0x02);
    outb(0x3C5, 0x0F);
    
    /* Set the color in the Set/Reset register */
    outb(0x3CE, 0x00);
    outb(0x3CF, color);
    
    /* Enable Set/Reset for all planes */
    outb(0x3CE, 0x01);
    outb(0x3CF, 0x0F);
    
    /* Set bit mask for single pixel */
    outb(0x3CE, 0x08);
    outb(0x3CF, mask);
    
    /* Read to latch, then write back */
    dummy = vga[offset];
    vga[offset] = dummy;
    
    /* Restore defaults */
    outb(0x3CE, 0x01);  /* Disable Set/Reset */
    outb(0x3CF, 0x00);
    outb(0x3CE, 0x08);  /* Reset bit mask */
    outb(0x3CF, 0xFF);
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
            /* Read to latch, then write back latched value */
            dummy = vga[offset + start_byte];
            vga[offset + start_byte] = dummy;  /* Write back latched value */
        } else {
            /* First byte (partial) */
            if (x1 & 7) {  /* Only if not byte-aligned */
                mask = 0xFF >> (x1 & 7);
                outb(0x3CE, 0x08);  /* Bit Mask Register */
                outb(0x3CF, mask);
                dummy = vga[offset + start_byte];
                vga[offset + start_byte] = dummy;  /* Write back latched value */
                start_byte++;
            }
            
            /* Middle bytes (full bytes) - only if there are bytes between first and last */
            if (start_byte < end_byte) {  /* Changed condition - now correctly checks for middle bytes */
                outb(0x3CE, 0x08);  /* Bit Mask Register */
                outb(0x3CF, 0xFF);  /* All pixels in byte */
                /* Use memset for faster filling of middle bytes */
                memset(&vga[offset + start_byte], 0x00, end_byte - start_byte);
            }
            
            /* Last byte (partial or full) */
            if ((x2 & 7) != 7) {  /* Last byte is partial */
                mask = 0xFF << (7 - (x2 & 7));
                outb(0x3CE, 0x08);  /* Bit Mask Register */
                outb(0x3CF, mask);
                dummy = vga[offset + end_byte];
                vga[offset + end_byte] = dummy;  /* Write back latched value */
            } else {
                /* Last byte is full */
                outb(0x3CE, 0x08);  /* Bit Mask Register */
                outb(0x3CF, 0xFF);  /* All pixels */
                vga[offset + end_byte] = 0x00;  /* Convention: write 0 for full bytes */
            }
        }
    }
    
    /* Restore default state */
    outb(0x3CE, 0x01);  /* Enable Set/Reset Register */
    outb(0x3CF, 0x00);  /* Disable Set/Reset */
    outb(0x3CE, 0x08);  /* Bit Mask Register */
    outb(0x3CF, 0xFF);  /* Enable all bits */
}

/* Simple abs implementation for freestanding environment */
static int abs(int x) {
    return x < 0 ? -x : x;
}

void draw_line(int x0, int y0, int x1, int y1, unsigned char color) {
    int dx = abs(x1 - x0);
    int dy = abs(y1 - y0);
    int sx = (x0 < x1) ? 1 : -1;
    int sy = (y0 < y1) ? 1 : -1;
    int err = dx - dy;
    int e2;
    
    while (1) {
        set_pixel(x0, y0, color);
        
        if (x0 == x1 && y0 == y1) break;
        
        e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x0 += sx;
        }
        if (e2 < dx) {
            err += dx;
            y0 += sy;
        }
    }
}

void draw_rectangle_outline(int x, int y, int width, int height, unsigned char color) {
    draw_line(x, y, x + width - 1, y, color);                    /* Top */
    draw_line(x, y + height - 1, x + width - 1, y + height - 1, color); /* Bottom */
    draw_line(x, y, x, y + height - 1, color);                   /* Left */
    draw_line(x + width - 1, y, x + width - 1, y + height - 1, color);  /* Right */
}

void draw_circle(int cx, int cy, int radius, unsigned char color) {
    int x = 0;
    int y = radius;
    int d = 3 - 2 * radius;
    
    while (x <= y) {
        /* Draw 8 octants */
        set_pixel(cx + x, cy + y, color);
        set_pixel(cx - x, cy + y, color);
        set_pixel(cx + x, cy - y, color);
        set_pixel(cx - x, cy - y, color);
        set_pixel(cx + y, cy + x, color);
        set_pixel(cx - y, cy + x, color);
        set_pixel(cx + y, cy - x, color);
        set_pixel(cx - y, cy - x, color);
        
        if (d < 0) {
            d = d + 4 * x + 6;
        } else {
            d = d + 4 * (x - y) + 10;
            y--;
        }
        x++;
    }
}

/* Check if character is a box-drawing character */
static int is_box_drawing(unsigned char c) {
    return (c >= 0xB0 && c <= 0xDF);  /* Box drawing range in CP437 */
}

/* Draw character with proper spacing and optional background */
void draw_char_extended(int x, int y, unsigned char c, 
                        unsigned char fg, unsigned char bg,
                        int char_spacing) {
    unsigned char *char_data;
    int row, col;
    unsigned char byte;
    int extend_8th;
    
    if (saved_font == NULL) {
        return;  /* No font available */
    }
    
    /* In VGA, each character is 32 bytes (16 rows, with padding) */
    char_data = saved_font + (c * 32);
    extend_8th = is_box_drawing(c);
    
    for (row = 0; row < CHAR_HEIGHT; row++) {
        byte = char_data[row];
        
        /* Draw normal 8 columns */
        for (col = 0; col < 8; col++) {
            if (byte & (0x80 >> col)) {
                set_pixel(x + col, y + row, fg);
            } else if (bg != COLOR_TRANSPARENT) {
                set_pixel(x + col, y + row, bg);
            }
        }
        
        /* Handle spacing beyond 8 pixels */
        if (char_spacing > 8) {
            /* 9th column: extend 8th for box chars, background otherwise */
            if (extend_8th && (byte & 0x01)) {
                set_pixel(x + 8, y + row, fg);
            } else if (bg != COLOR_TRANSPARENT) {
                set_pixel(x + 8, y + row, bg);
            }
            
            /* Fill any additional spacing with background */
            for (col = 9; col < char_spacing; col++) {
                if (bg != COLOR_TRANSPARENT) {
                    set_pixel(x + col, y + row, bg);
                }
            }
        }
    }
}

/* Legacy function - now calls extended version */
void draw_char_from_bios_font(int x, int y, unsigned char c, unsigned char color) {
    draw_char_extended(x, y, c, color, COLOR_TRANSPARENT, CHAR_WIDTH_TIGHT);
}

/* Draw text with configurable spacing and background */
void draw_text_spaced(int x, int y, const char *text, 
                     unsigned char fg, unsigned char bg,
                     int char_spacing) {
    int orig_x = x;
    const char *p = text;
    
    if (saved_font == NULL) {
        return;  /* No font available */
    }
    
    while (*p) {
        if (*p == '\n') {
            /* Handle newline */
            x = orig_x;
            y += default_font.line_height;
        } else if (*p == '\t') {
            /* Align to next tab stop (every 8 characters) */
            int chars_from_start = (x - orig_x) / char_spacing;
            int next_tab = ((chars_from_start / 8) + 1) * 8;
            x = orig_x + (next_tab * char_spacing);
        } else {
            /* Draw the character with proper spacing */
            draw_char_extended(x, y, (unsigned char)*p, fg, bg, char_spacing);
            x += char_spacing;
        }
        p++;
    }
}

/* Legacy draw_string - now uses proper spacing */
void draw_string(int x, int y, const char *str, unsigned char color) {
    draw_text_spaced(x, y, str, color, COLOR_TRANSPARENT, CHAR_WIDTH_NORMAL);
}

/* Text metrics helper functions */

/* Convert text column/row to pixel coordinates */
void text_pos_to_pixels(int col, int row, int *x, int *y) {
    *x = col * default_font.char_width;
    *y = row * default_font.line_height;
}

/* Get string width in pixels (single line only) */
int get_text_width(const char *str) {
    int width = 0;
    while (*str && *str != '\n') {
        width += default_font.char_width;
        str++;
    }
    return width;
}

/* Center text horizontally */
void draw_text_centered(int y, const char *text, 
                        unsigned char fg, unsigned char bg) {
    int width = get_text_width(text);
    int x = (VGA_WIDTH_12H - width) / 2;
    draw_text_spaced(x, y, text, fg, bg, default_font.char_width);
}

/* Draw text right-aligned */
void draw_text_right_aligned(int right_x, int y, const char *text,
                             unsigned char fg, unsigned char bg) {
    int width = get_text_width(text);
    int x = right_x - width;
    draw_text_spaced(x, y, text, fg, bg, default_font.char_width);
}

/* Save background pixels where cursor will be drawn (including outline area) */
static void save_cursor_background(int x, int y) {
    int row, col;
    int px, py;
    int bg_index = 0;
    int save_width = mouse_cursor.width + 2;   /* Add 1 pixel border on each side */
    int save_height = mouse_cursor.height + 2;
    
    /* Save the position where background is being saved */
    mouse_cursor.saved_x = x;
    mouse_cursor.saved_y = y;
    
    /* Save a slightly larger area to include outline */
    for (row = -1; row <= mouse_cursor.height; row++) {
        for (col = -1; col <= mouse_cursor.width; col++) {
            px = x + col - mouse_cursor.hotspot_x;
            py = y + row - mouse_cursor.hotspot_y;
            
            /* Save all pixels in the extended cursor rectangle */
            if (px >= 0 && px < VGA_WIDTH_12H && 
                py >= 0 && py < VGA_HEIGHT_12H) {
                mouse_cursor.saved_background[bg_index] = read_pixel(px, py);
            } else {
                mouse_cursor.saved_background[bg_index] = COLOR_BACKGROUND; /* Use background color */
            }
            bg_index++;
        }
    }
}

/* Restore background pixels where cursor was drawn */
static void restore_cursor_background(void) {
    int row, col;
    int px, py;
    int bg_index = 0;
    
    /* Only restore if we have a valid saved position */
    if (mouse_cursor.saved_x < 0 || mouse_cursor.saved_y < 0) {
        return;
    }
    
    /* Restore the extended area (including outline) */
    for (row = -1; row <= mouse_cursor.height; row++) {
        for (col = -1; col <= mouse_cursor.width; col++) {
            px = mouse_cursor.saved_x + col - mouse_cursor.hotspot_x;
            py = mouse_cursor.saved_y + row - mouse_cursor.hotspot_y;
            
            /* Restore pixels that were within screen bounds */
            if (px >= 0 && px < VGA_WIDTH_12H && 
                py >= 0 && py < VGA_HEIGHT_12H) {
                set_pixel(px, py, mouse_cursor.saved_background[bg_index]);
            }
            bg_index++;
        }
    }
}

/* Draw mouse cursor with black outline for better visibility */
static void draw_cursor(int x, int y) {
    int row, col;
    int byte_index, bit_index;
    int px, py;
    int dx, dy;
    
    /* First pass: Draw black outline (1-pixel border around cursor shape) */
    for (row = 0; row < mouse_cursor.height; row++) {
        for (col = 0; col < mouse_cursor.width; col++) {
            byte_index = (row * 2) + (col / 8);
            bit_index = 7 - (col % 8);
            
            if (mouse_cursor.bitmap[byte_index] & (1 << bit_index)) {
                /* Check all 8 neighbors for outline */
                for (dy = -1; dy <= 1; dy++) {
                    for (dx = -1; dx <= 1; dx++) {
                        if (dx == 0 && dy == 0) continue;
                        
                        px = x + col + dx - mouse_cursor.hotspot_x;
                        py = y + row + dy - mouse_cursor.hotspot_y;
                        
                        /* Draw black outline pixel if in bounds */
                        if (px >= 0 && px < VGA_WIDTH_12H && 
                            py >= 0 && py < VGA_HEIGHT_12H) {
                            /* Check if neighbor is outside cursor shape */
                            int n_col = col + dx;
                            int n_row = row + dy;
                            if (n_col < 0 || n_col >= mouse_cursor.width || 
                                n_row < 0 || n_row >= mouse_cursor.height) {
                                set_pixel(px, py, 0x00); /* Black outline */
                            } else {
                                int n_byte = (n_row * 2) + (n_col / 8);
                                int n_bit = 7 - (n_col % 8);
                                if (!(mouse_cursor.bitmap[n_byte] & (1 << n_bit))) {
                                    set_pixel(px, py, 0x00); /* Black outline */
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    
    /* Second pass: Draw white cursor body */
    for (row = 0; row < mouse_cursor.height; row++) {
        for (col = 0; col < mouse_cursor.width; col++) {
            byte_index = (row * 2) + (col / 8);
            bit_index = 7 - (col % 8);
            
            if (mouse_cursor.bitmap[byte_index] & (1 << bit_index)) {
                px = x + col - mouse_cursor.hotspot_x;
                py = y + row - mouse_cursor.hotspot_y;
                
                if (px >= 0 && px < VGA_WIDTH_12H && 
                    py >= 0 && py < VGA_HEIGHT_12H) {
                    set_pixel(px, py, 0x0F); /* White cursor */
                }
            }
        }
    }
}

/* Flag to temporarily disable cursor during critical updates */
static int cursor_update_suspended = 0;

/* Initialize mouse cursor */
void init_mouse_cursor(void) {
    mouse_cursor.visible = 0;
    mouse_cursor.x = VGA_WIDTH_12H / 2;
    mouse_cursor.y = VGA_HEIGHT_12H / 2;
}

/* Show the mouse cursor */
void show_mouse_cursor(void) {
    if (!mouse_cursor.visible) {
        save_cursor_background(mouse_cursor.x, mouse_cursor.y);
        draw_cursor(mouse_cursor.x, mouse_cursor.y);
        mouse_cursor.visible = 1;
    }
}

/* Hide the mouse cursor */
void hide_mouse_cursor(void) {
    if (mouse_cursor.visible) {
        restore_cursor_background();
        mouse_cursor.visible = 0;
        mouse_cursor.saved_x = -1;
        mouse_cursor.saved_y = -1;
    }
}

/* Update mouse cursor position */
void update_mouse_cursor(int new_x, int new_y) {
    /* Clip to screen bounds first */
    if (new_x < 0) new_x = 0;
    if (new_y < 0) new_y = 0;
    if (new_x >= VGA_WIDTH_12H) new_x = VGA_WIDTH_12H - 1;
    if (new_y >= VGA_HEIGHT_12H) new_y = VGA_HEIGHT_12H - 1;
    
    /* Only update if position changed */
    if (new_x == mouse_cursor.x && new_y == mouse_cursor.y) {
        return;
    }
    
    /* Skip updates if cursor is suspended */
    if (cursor_update_suspended) {
        mouse_cursor.x = new_x;
        mouse_cursor.y = new_y;
        return;
    }
    
    if (mouse_cursor.visible) {
        /* Restore old background */
        restore_cursor_background();
        
        /* Update position */
        mouse_cursor.x = new_x;
        mouse_cursor.y = new_y;
        
        /* Save new background and draw cursor */
        save_cursor_background(mouse_cursor.x, mouse_cursor.y);
        draw_cursor(mouse_cursor.x, mouse_cursor.y);
    } else {
        /* Just update position if not visible */
        mouse_cursor.x = new_x;
        mouse_cursor.y = new_y;
    }
}

/* Get current mouse cursor position */
void get_mouse_cursor_pos(int *x, int *y) {
    *x = mouse_cursor.x;
    *y = mouse_cursor.y;
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

/* Global flag to indicate graphics mode is active */
int graphics_mode_active = 0;

/* Separate pixel-level mouse coordinates for graphics mode */
static int graphics_mouse_x = 320;
static int graphics_mouse_y = 240;
static float mouse_x_accumulator = 0.0;
static float mouse_y_accumulator = 0.0;

/* Handle raw mouse movement in graphics mode - called from kernel.c 
 * Takes raw mouse deltas instead of text coordinates */
void handle_graphics_mouse_raw(signed char dx, signed char dy) {
    float scale = 1.5;  /* Sensitivity adjustment */
    int move_x, move_y;
    
    if (!graphics_mode_active) return;
    
    /* Accumulate fractional movement for smoother motion */
    mouse_x_accumulator += dx * scale;
    mouse_y_accumulator += dy * scale;  /* Positive dy moves down */
    
    /* Extract integer pixels to move */
    move_x = (int)mouse_x_accumulator;
    move_y = (int)mouse_y_accumulator;
    
    /* Keep fractional part */
    mouse_x_accumulator -= move_x;
    mouse_y_accumulator -= move_y;
    
    /* Update position with bounds checking */
    graphics_mouse_x += move_x;
    graphics_mouse_y += move_y;
    
    if (graphics_mouse_x < 0) graphics_mouse_x = 0;
    if (graphics_mouse_x >= VGA_WIDTH_12H) graphics_mouse_x = VGA_WIDTH_12H - 1;
    if (graphics_mouse_y < 0) graphics_mouse_y = 0;
    if (graphics_mouse_y >= VGA_HEIGHT_12H) graphics_mouse_y = VGA_HEIGHT_12H - 1;
    
    /* Update cursor */
    update_mouse_cursor(graphics_mouse_x, graphics_mouse_y);
}

/* Legacy function for compatibility */
void handle_graphics_mouse_move(int text_x, int text_y) {
    /* Not used anymore - we use raw mouse input instead */
    (void)text_x;
    (void)text_y;
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
    extern int mouse_x, mouse_y;  /* Access text-mode mouse coords from kernel.c */
    extern void poll_mouse(void);  /* Mouse polling function from kernel.c */
    
    /* Frame timing constants */
    #define FRAME_DELAY_MS 50  /* 50ms = 20 FPS */
    
    /* Save font before switching to graphics */
    save_vga_font();
    
    set_mode_12h();
    
    /* Set our custom Aquinas palette */
    set_aquinas_palette();
    
    /* Clear screen with medium gray background */
    clear_graphics_screen(COLOR_BACKGROUND);
    
    /* Draw title text */
    draw_string(20, 5, "Aquinas Graphics Mode Demo", COLOR_TEXT);
    
    /* Draw UI demo with the new palette */
    /* Grayscale showcase */
    draw_rectangle(20, 20, 60, 60, 0);   /* Black */
    draw_rectangle(90, 20, 60, 60, 1);   /* Dark gray */
    draw_rectangle(160, 20, 60, 60, 2);  /* Medium dark gray */
    draw_rectangle(230, 20, 60, 60, 3);  /* Medium gray */
    draw_rectangle(300, 20, 60, 60, 4);  /* Light gray */
    draw_rectangle(370, 20, 60, 60, 5);  /* White */
    
    /* Red showcase */
    draw_string(20, 85, "Reds:", COLOR_TEXT);
    draw_rectangle(20, 100, 100, 50, 6);   /* Dark red */
    draw_rectangle(130, 100, 100, 50, 7);  /* Medium red */
    draw_rectangle(240, 100, 100, 50, 8);  /* Bright red */
    
    /* Gold showcase */
    draw_string(20, 155, "Golds:", COLOR_TEXT);
    draw_rectangle(20, 170, 100, 50, 9);   /* Dark gold */
    draw_rectangle(130, 170, 100, 50, 10); /* Medium gold */
    draw_rectangle(240, 170, 100, 50, 11); /* Bright yellow-gold */
    
    /* Cyan showcase */
    draw_string(20, 225, "Cyans:", COLOR_TEXT);
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
    
    /* New drawing primitives demo */
    draw_string(360, 320, "Drawing Primitives:", COLOR_TEXT);
    /* Lines in various directions */
    draw_line(360, 380, 440, 380, COLOR_TEXT);        /* Horizontal */
    draw_line(400, 340, 400, 420, COLOR_TEXT);        /* Vertical */
    draw_line(360, 340, 440, 420, COLOR_LINK);        /* Diagonal \ */
    draw_line(360, 420, 440, 340, COLOR_COMMAND);     /* Diagonal / */
    
    /* Rectangle outlines */
    draw_rectangle_outline(460, 360, 80, 60, COLOR_HIGHLIGHT);
    draw_rectangle_outline(470, 370, 60, 40, COLOR_CURSOR);
    
    /* Circles of different sizes */
    draw_circle(560, 380, 30, COLOR_LINK);
    draw_circle(560, 380, 20, COLOR_COMMAND);
    draw_circle(560, 380, 10, COLOR_SELECTION);
    
    /* Instructions */
    draw_string(20, 460, "Press ESC to exit graphics mode", COLOR_TEXT_DIM);
    
    /* Initialize mouse cursor */
    init_mouse_cursor();
    graphics_mode_active = 1;  /* Set flag for mouse handler */
    
    /* Reset graphics mouse position to center */
    graphics_mouse_x = VGA_WIDTH_12H / 2;
    graphics_mouse_y = VGA_HEIGHT_12H / 2;
    mouse_x_accumulator = 0.0;
    mouse_y_accumulator = 0.0;
    
    show_mouse_cursor();
    
    /* Initialize timing */
    last_frame_time = get_ticks();
    
    while (running) {
        unsigned char scancode;
        
        /* Poll for mouse movement */
        poll_mouse();
        
        /* Check for keyboard input - only handle ESC to exit */
        scancode = inb(0x60);
        
        if (scancode == 0x01) { /* ESC - exit */
            running = 0;
        }
        
        /* Get current time */
        current_time = get_ticks();
        
        /* Update animation only if enough time has passed */
        if (current_time - last_frame_time >= FRAME_DELAY_MS) {
            /* Temporarily hide cursor during animation update */
            if (mouse_cursor.visible) {
                restore_cursor_background();
                cursor_update_suspended = 1;
            }
            
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
            
            /* Re-enable cursor and redraw it */
            if (mouse_cursor.visible) {
                cursor_update_suspended = 0;
                save_cursor_background(mouse_cursor.x, mouse_cursor.y);
                draw_cursor(mouse_cursor.x, mouse_cursor.y);
            }
            
            /* Update last frame time */
            last_frame_time = current_time;
        }
    }
    
    /* Hide cursor before switching modes */
    hide_mouse_cursor();
    graphics_mode_active = 0;  /* Clear flag */
    
    set_mode_03h();
    
    /* Restore font after switching back to text mode */
    restore_vga_font();
    
    /* Restore DAC palette for proper colors */
    restore_dac_palette();
}
