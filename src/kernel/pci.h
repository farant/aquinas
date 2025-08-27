#ifndef PCI_H
#define PCI_H

/* PCI Configuration Space Ports */
#define PCI_CONFIG_ADDRESS  0xCF8
#define PCI_CONFIG_DATA     0xCFC

/* PCI Device Classes */
#define PCI_CLASS_DISPLAY   0x03

/* Known VGA device vendor IDs */
#define PCI_VENDOR_BOCHS    0x1234  /* QEMU Standard VGA */
#define PCI_VENDOR_VMWARE   0x15AD  /* VMware SVGA */
#define PCI_VENDOR_REDHAT   0x1B36  /* VirtIO */

/* PCI BAR indices */
#define PCI_BAR0            0x10
#define PCI_BAR1            0x14

/* PCI functions */
unsigned int pci_config_read(unsigned char bus, unsigned char device, unsigned char func, unsigned char offset);
void pci_config_write(unsigned char bus, unsigned char device, unsigned char func, unsigned char offset, unsigned int value);
unsigned int pci_find_vga_framebuffer(void);

#endif