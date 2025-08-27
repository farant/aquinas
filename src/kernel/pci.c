/* PCI Configuration Space Access
 *
 * This module provides basic PCI configuration space access to detect
 * the VGA device and find its framebuffer address from BAR0.
 */

#include "pci.h"
#include "io.h"
#include "serial.h"

/* Read from PCI configuration space */
unsigned int pci_config_read(unsigned char bus, unsigned char device, unsigned char func, unsigned char offset) {
    unsigned int address;
    unsigned int value;
    
    /* Create configuration address */
    address = (unsigned int)((bus << 16) | (device << 11) | (func << 8) | 
                             (offset & 0xFC) | 0x80000000);
    
    /* Write address to CONFIG_ADDRESS */
    outl(PCI_CONFIG_ADDRESS, address);
    
    /* Read data from CONFIG_DATA */
    value = inl(PCI_CONFIG_DATA);
    
    return value;
}

/* Write to PCI configuration space */
void pci_config_write(unsigned char bus, unsigned char device, unsigned char func, 
                      unsigned char offset, unsigned int value) {
    unsigned int address;
    
    /* Create configuration address */
    address = (unsigned int)((bus << 16) | (device << 11) | (func << 8) | 
                             (offset & 0xFC) | 0x80000000);
    
    /* Write address to CONFIG_ADDRESS */
    outl(PCI_CONFIG_ADDRESS, address);
    
    /* Write data to CONFIG_DATA */
    outl(PCI_CONFIG_DATA, value);
}

/* Find VGA device and return framebuffer address */
unsigned int pci_find_vga_framebuffer(void) {
    unsigned char bus, device;
    unsigned int vendor_device;
    unsigned int class_code;
    unsigned int bar0, bar1;
    unsigned int framebuffer = 0;
    
    serial_write_string("Scanning PCI for VGA device...\n");
    
    /* Scan PCI bus 0 for VGA devices */
    for (device = 0; device < 32; device++) {
        /* Read vendor and device ID */
        vendor_device = pci_config_read(0, device, 0, 0);
        
        /* Check if device exists (vendor != 0xFFFF) */
        if ((vendor_device & 0xFFFF) == 0xFFFF) {
            continue;
        }
        
        /* Read class code */
        class_code = pci_config_read(0, device, 0, 0x08);
        class_code = (class_code >> 16) & 0xFFFF;  /* Get class and subclass */
        
        /* Check if it's a VGA device (class 0x03, subclass 0x00) */
        if ((class_code >> 8) == PCI_CLASS_DISPLAY) {
            unsigned short vendor_id = vendor_device & 0xFFFF;
            unsigned short device_id = (vendor_device >> 16) & 0xFFFF;
            
            serial_write_string("Found VGA device: vendor=");
            serial_write_hex(vendor_id);
            serial_write_string(" device=");
            serial_write_hex(device_id);
            serial_write_string("\n");
            
            /* Read BAR0 */
            bar0 = pci_config_read(0, device, 0, PCI_BAR0);
            
            /* Check which BAR to use based on vendor */
            if (vendor_id == PCI_VENDOR_VMWARE) {
                /* VMware uses BAR1 for framebuffer */
                bar1 = pci_config_read(0, device, 0, PCI_BAR1);
                framebuffer = bar1 & 0xFFFFFFF0;  /* Clear lower bits */
                serial_write_string("VMware VGA detected, using BAR1\n");
            } else {
                /* Standard VGA and others use BAR0 */
                framebuffer = bar0 & 0xFFFFFFF0;  /* Clear lower bits */
                serial_write_string("Standard VGA detected, using BAR0\n");
            }
            
            serial_write_string("Framebuffer address: ");
            serial_write_hex(framebuffer);
            serial_write_string("\n");
            
            /* Return first VGA device found */
            return framebuffer;
        }
    }
    
    serial_write_string("No VGA device found on PCI bus\n");
    
    /* Return default if no VGA device found */
    return 0xE0000000;
}