#ifndef DISPI_H
#define DISPI_H

/* DISPI (Display Interface) driver for Bochs/QEMU VGA
 * Provides linear framebuffer access for 640x480 8bpp mode
 */

/* DISPI I/O ports */
#define VBE_DISPI_IOPORT_INDEX          0x01CE
#define VBE_DISPI_IOPORT_DATA           0x01CF

/* DISPI register indices */
#define VBE_DISPI_INDEX_ID              0x0
#define VBE_DISPI_INDEX_XRES            0x1
#define VBE_DISPI_INDEX_YRES            0x2
#define VBE_DISPI_INDEX_BPP             0x3
#define VBE_DISPI_INDEX_ENABLE          0x4
#define VBE_DISPI_INDEX_BANK            0x5
#define VBE_DISPI_INDEX_VIRT_WIDTH      0x6
#define VBE_DISPI_INDEX_VIRT_HEIGHT     0x7
#define VBE_DISPI_INDEX_X_OFFSET        0x8
#define VBE_DISPI_INDEX_Y_OFFSET        0x9
#define VBE_DISPI_INDEX_VIDEO_MEMORY_64K 0xa

/* DISPI ID values */
#define VBE_DISPI_ID0                   0xB0C0
#define VBE_DISPI_ID1                   0xB0C1
#define VBE_DISPI_ID2                   0xB0C2
#define VBE_DISPI_ID3                   0xB0C3
#define VBE_DISPI_ID4                   0xB0C4
#define VBE_DISPI_ID5                   0xB0C5

/* DISPI enable flags */
#define VBE_DISPI_DISABLED              0x00
#define VBE_DISPI_ENABLED               0x01
#define VBE_DISPI_GETCAPS               0x02
#define VBE_DISPI_8BIT_DAC              0x20
#define VBE_DISPI_LFB_ENABLED           0x40
#define VBE_DISPI_NOCLEARMEM            0x80

/* Default framebuffer address */
#define DISPI_LFB_PHYSICAL_ADDRESS      0xE0000000

/* Our target resolution */
#define DISPI_WIDTH                     640
#define DISPI_HEIGHT                    480
#define DISPI_BPP                       8

/* DISPI functions */
void dispi_write(unsigned short index, unsigned short value);
unsigned short dispi_read(unsigned short index);
int dispi_detect(void);
void dispi_init(void);
void dispi_set_mode(int width, int height, int bpp);
void dispi_enable(int lfb_enable);
void dispi_disable(void);
unsigned char* dispi_get_framebuffer(void);
unsigned int dispi_get_framebuffer_size(void);

/* Get the display driver for DISPI */
struct DisplayDriver* dispi_get_driver(void);

#endif