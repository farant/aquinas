/* DISPI Graphics Mode Initialization
 * 
 * Common initialization for DISPI graphics mode used by all demos.
 */

#ifndef DISPI_INIT_H
#define DISPI_INIT_H

#include "display_driver.h"
#include "graphics_context.h"

/* Initialize DISPI graphics mode with standard Aquinas palette */
DisplayDriver* dispi_graphics_init(void);

/* Cleanup and return to text mode */
void dispi_graphics_cleanup(GraphicsContext *gc);

/* Get the standard Aquinas palette */
void dispi_get_aquinas_palette(unsigned char palette[16][3]);

#endif /* DISPI_INIT_H */