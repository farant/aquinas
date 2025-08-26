/* TTF to C Array Converter using stb_truetype
 * Compile: gcc -o ttf_to_c ttf_to_c_stb.c -lm
 * Usage: ./ttf_to_c font.ttf 6 8 > font_6x8.h
 */

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char *argv[]) {
    if (argc < 4) {
        fprintf(stderr, "Usage: %s <font.ttf> <width> <height> [name]\n", argv[0]);
        fprintf(stderr, "Example: %s myfont.ttf 6 8 tiny\n", argv[0]);
        return 1;
    }
    
    const char *fontfile = argv[1];
    int char_width = atoi(argv[2]);
    int char_height = atoi(argv[3]);
    const char *font_name = argc > 4 ? argv[4] : "custom";
    
    /* Read font file */
    FILE *f = fopen(fontfile, "rb");
    if (!f) {
        fprintf(stderr, "Cannot open font file: %s\n", fontfile);
        return 1;
    }
    
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    unsigned char *fontBuffer = malloc(size);
    fread(fontBuffer, 1, size, f);
    fclose(f);
    
    /* Initialize stb_truetype */
    stbtt_fontinfo font;
    if (!stbtt_InitFont(&font, fontBuffer, stbtt_GetFontOffsetForIndex(fontBuffer, 0))) {
        fprintf(stderr, "Failed to initialize font\n");
        free(fontBuffer);
        return 1;
    }
    
    /* Calculate scale for desired pixel height */
    float scale = stbtt_ScaleForPixelHeight(&font, char_height);
    
    /* Get font vertical metrics */
    int ascent, descent, lineGap;
    stbtt_GetFontVMetrics(&font, &ascent, &descent, &lineGap);
    int baseline = (int)(ascent * scale);
    
    /* Output header */
    printf("/* Bitmap font generated from %s */\n", fontfile);
    printf("/* Character size: %dx%d pixels */\n", char_width, char_height);
    printf("/* Generated using stb_truetype.h */\n\n");
    
    printf("#ifndef FONT_%s_%dx%d_H\n", font_name, char_width, char_height);
    printf("#define FONT_%s_%dx%d_H\n\n", font_name, char_width, char_height);
    
    printf("#define FONT_%s_WIDTH %d\n", font_name, char_width);
    printf("#define FONT_%s_HEIGHT %d\n\n", font_name, char_height);
    
    printf("static const unsigned char font_%s_%dx%d[256][%d] = {\n", 
           font_name, char_width, char_height, char_height);
    
    /* Process each character */
    for (int ch = 0; ch < 256; ch++) {
        unsigned char bitmap[char_height][char_width];
        memset(bitmap, 0, sizeof(bitmap));
        
        /* Render character if printable */
        if (ch >= 32) {
            int w, h, xoff, yoff;
            unsigned char *mono_bitmap = stbtt_GetCodepointBitmap(
                &font, 0, scale, ch, &w, &h, &xoff, &yoff
            );
            
            if (mono_bitmap) {
                /* Calculate position to center character */
                int x_start = (char_width - w) / 2 + xoff;
                int y_start = baseline + yoff;
                
                /* Copy rendered character to our bitmap */
                for (int y = 0; y < h && y + y_start < char_height; y++) {
                    for (int x = 0; x < w && x + x_start < char_width; x++) {
                        if (y + y_start >= 0 && x + x_start >= 0) {
                            /* Threshold at 127 for 1-bit output */
                            if (mono_bitmap[y * w + x] > 127) {
                                bitmap[y + y_start][x + x_start] = 1;
                            }
                        }
                    }
                }
                
                stbtt_FreeBitmap(mono_bitmap, NULL);
            }
        }
        
        /* Convert bitmap to hex bytes */
        printf("    /* 0x%02X ", ch);
        if (ch >= 32 && ch < 127) {
            printf("'%c'", ch);
        }
        printf(" */\n");
        printf("    {");
        
        for (int row = 0; row < char_height; row++) {
            unsigned char byte = 0;
            for (int col = 0; col < char_width && col < 8; col++) {
                if (bitmap[row][col]) {
                    byte |= (0x80 >> col);
                }
            }
            printf("0x%02X", byte);
            if (row < char_height - 1) printf(", ");
        }
        
        printf("}");
        if (ch < 255) printf(",");
        printf("\n");
    }
    
    printf("};\n\n");
    printf("#endif /* FONT_%s_%dx%d_H */\n", font_name, char_width, char_height);
    
    free(fontBuffer);
    return 0;
}