/* UI Theme and Color Definitions
 * 
 * Standard colors for the Aquinas UI component library.
 * Based on the warm gray background from DISPI demo.
 */

#ifndef UI_THEME_H
#define UI_THEME_H

/* Color Palette - matches the Aquinas palette from dispi_demo */
#define COLOR_BLACK         0   /* 0x00, 0x00, 0x00 */
#define COLOR_DARK_GRAY     1   /* 0x40, 0x40, 0x40 */
#define COLOR_MED_DARK_GRAY 2   /* 0x80, 0x80, 0x80 */
#define COLOR_MED_GRAY      3   /* 0xC0, 0xC0, 0xC0 */
#define COLOR_LIGHT_GRAY    4   /* 0xE0, 0xE0, 0xE0 */
#define COLOR_WHITE         5   /* 0xFC, 0xFC, 0xFC */

#define COLOR_DARK_RED      6   /* 0x80, 0x20, 0x20 */
#define COLOR_MED_RED       7   /* 0xC0, 0x30, 0x30 */
#define COLOR_BRIGHT_RED    8   /* 0xFC, 0x40, 0x40 */

#define COLOR_DARK_GOLD     9   /* 0xA0, 0x80, 0x20 */
#define COLOR_MED_GOLD      10  /* 0xE0, 0xC0, 0x40 */
#define COLOR_BRIGHT_GOLD   11  /* 0xFC, 0xE0, 0x60 */

#define COLOR_DARK_CYAN     12  /* 0x20, 0x80, 0xA0 */
#define COLOR_MED_CYAN      13  /* 0x40, 0xC0, 0xE0 */
#define COLOR_BRIGHT_CYAN   14  /* 0x60, 0xE0, 0xFC */

#define COLOR_WARM_GRAY     15  /* 0xB0, 0xA0, 0x80 - Default background */

/* Theme Colors - Semantic assignments */
#define THEME_BG            COLOR_WARM_GRAY    /* Default background */
#define THEME_FG            COLOR_BLACK        /* Default text */
#define THEME_BORDER        COLOR_MED_DARK_GRAY /* Default borders */
#define THEME_SHADOW        COLOR_DARK_GRAY    /* Shadows and depth */

/* Component backgrounds */
#define THEME_BUTTON_BG     COLOR_LIGHT_GRAY   /* Normal button */
#define THEME_BUTTON_HOVER  COLOR_WHITE        /* Hovered button */
#define THEME_BUTTON_PRESS  COLOR_MED_GRAY     /* Pressed button */
#define THEME_INPUT_BG      COLOR_WHITE        /* Text input background */
#define THEME_PANEL_BG      COLOR_LIGHT_GRAY   /* Panel background */

/* Accent colors for emphasis */
#define THEME_ACCENT_CYAN   COLOR_MED_CYAN     /* Info, selection */
#define THEME_ACCENT_RED    COLOR_MED_RED      /* Errors, danger */
#define THEME_ACCENT_GOLD   COLOR_MED_GOLD     /* Warnings, highlights */

/* Focus and selection */
#define THEME_FOCUS         COLOR_BRIGHT_CYAN  /* Focus ring */
#define THEME_SELECTION     COLOR_DARK_CYAN    /* Selected items */
#define THEME_CURSOR        COLOR_BLACK        /* Text cursor */

/* Font selection */
typedef enum {
    FONT_6X8,   /* Compact font for UI elements */
    FONT_9X16   /* BIOS font for content areas */
} FontSize;

/* Spacing constants */
#define PADDING_SMALL   2
#define PADDING_MEDIUM  4
#define PADDING_LARGE   8

#define MARGIN_SMALL    2
#define MARGIN_MEDIUM   4
#define MARGIN_LARGE    8

/* Border styles */
typedef enum {
    BORDER_NONE,
    BORDER_FLAT,    /* Single line */
    BORDER_RAISED,  /* 3D raised effect */
    BORDER_SUNKEN   /* 3D sunken effect */
} BorderStyle;

#endif /* UI_THEME_H */