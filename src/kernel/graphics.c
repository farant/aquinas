#include "graphics.h"
#include "io.h"
#include "serial.h"
#include "memory.h"

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
    
    /* First, do a sequencer reset */
    outb(0x3C4, 0x00);
    outb(0x3C5, 0x01);  /* Assert reset */
    
    /* Miscellaneous Output Register */
    outb(0x3C2, 0x67);  /* 0x67 for 80x25 color text mode */
    
    /* Program sequencer - matching SeaBIOS sequ_03 */
    outb(0x3C4, 0x01); outb(0x3C5, 0x03);  /* 9 dot clocks per char - CRITICAL! */
    outb(0x3C4, 0x02); outb(0x3C5, 0x03);  /* Map mask - planes 0 and 1 for text */
    outb(0x3C4, 0x03); outb(0x3C5, 0x00);  /* Character map select */
    outb(0x3C4, 0x04); outb(0x3C5, 0x02);  /* Memory mode - odd/even addressing */
    
    /* Release sequencer reset */
    outb(0x3C4, 0x00);
    outb(0x3C5, 0x03);  /* De-assert reset */
    
    /* Unlock CRTC */
    outb(0x3D4, 0x11);
    tmp = inb(0x3D5);
    outb(0x3D5, tmp & 0x7F);
    
    for (i = 0; i < 25; i++) {
        outb(0x3D4, i);
        outb(0x3D5, crtc_vals[i]);
    }
    
    /* Graphics Controller - critical for text mode! */
    outb(0x3CE, 0x00); outb(0x3CF, 0x00);  /* Set/Reset */
    outb(0x3CE, 0x01); outb(0x3CF, 0x00);  /* Enable Set/Reset */
    outb(0x3CE, 0x02); outb(0x3CF, 0x00);  /* Color Compare */
    outb(0x3CE, 0x03); outb(0x3CF, 0x00);  /* Data Rotate */
    outb(0x3CE, 0x04); outb(0x3CF, 0x00);  /* Read Map Select */
    outb(0x3CE, 0x05); outb(0x3CF, 0x10);  /* Graphics Mode - odd/even, no chain */
    outb(0x3CE, 0x06); outb(0x3CF, 0x0E);  /* Memory Map Mode - B8000-BFFFF, text mode */
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
    outb(0x3C0, 0x08);  /* Graphics mode 0, text mode, blink enable */
    
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
    int plane, row, col;
    int x1, x2, y1, y2;
    unsigned int offset;
    unsigned char mask_left, mask_right, mask;
    
    if (x >= VGA_WIDTH_12H || y >= VGA_HEIGHT_12H) return;
    
    x1 = x;
    y1 = y;
    x2 = x + width - 1;
    y2 = y + height - 1;
    
    if (x2 >= VGA_WIDTH_12H) x2 = VGA_WIDTH_12H - 1;
    if (y2 >= VGA_HEIGHT_12H) y2 = VGA_HEIGHT_12H - 1;
    
    for (plane = 0; plane < 4; plane++) {
        outb(0x3C4, 0x02);
        outb(0x3C5, 1 << plane);
        
        for (row = y1; row <= y2; row++) {
            int start_byte = x1 / 8;
            int end_byte = x2 / 8;
            
            offset = row * (VGA_WIDTH_12H / 8);
            
            if (start_byte == end_byte) {
                mask_left = 0xFF >> (x1 & 7);
                mask_right = 0xFF << (7 - (x2 & 7));
                mask = mask_left & mask_right;
                
                if (color & (1 << plane)) {
                    vga[offset + start_byte] |= mask;
                } else {
                    vga[offset + start_byte] &= ~mask;
                }
            } else {
                mask_left = 0xFF >> (x1 & 7);
                if (color & (1 << plane)) {
                    vga[offset + start_byte] |= mask_left;
                } else {
                    vga[offset + start_byte] &= ~mask_left;
                }
                
                for (col = start_byte + 1; col < end_byte; col++) {
                    if (color & (1 << plane)) {
                        vga[offset + col] = 0xFF;
                    } else {
                        vga[offset + col] = 0x00;
                    }
                }
                
                mask_right = 0xFF << (7 - (x2 & 7));
                if (color & (1 << plane)) {
                    vga[offset + end_byte] |= mask_right;
                } else {
                    vga[offset + end_byte] &= ~mask_right;
                }
            }
        }
    }
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
    int i;
    
    /* Save font before switching to graphics */
    save_vga_font();
    
    set_mode_12h();
    
    clear_graphics_screen(0);
    
    draw_rectangle(50, 50, 100, 100, 1);
    draw_rectangle(200, 50, 150, 100, 2);
    draw_rectangle(400, 50, 100, 150, 3);
    
    draw_rectangle(50, 200, 200, 50, 4);
    draw_rectangle(300, 200, 100, 100, 5);
    draw_rectangle(450, 200, 150, 80, 6);
    
    draw_rectangle(100, 350, 80, 80, 9);
    draw_rectangle(250, 350, 120, 60, 10);
    draw_rectangle(400, 350, 160, 90, 12);
    
    draw_rectangle(260, 10, 120, 20, 15);
    
    while (running) {
        if (inb(0x60) == 0x01) {
            running = 0;
        }
        
        animation_frame++;
        if (animation_frame % 10000 == 0) {
            int color = (animation_frame / 10000) % 16;
            draw_rectangle(290 + (animation_frame/10000) % 40, 
                          240 + (animation_frame/10000) % 30, 
                          60, 40, color);
        }
    }
    
    set_mode_03h();
    
    /* Restore font after switching back to text mode */
    restore_vga_font();
    
    /* Restore DAC palette for proper colors */
    restore_dac_palette();
}