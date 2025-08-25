/* Timer implementation using the PIT (Programmable Interval Timer)
 *
 * The 8253/8254 PIT chip has 3 channels:
 * - Channel 0: Connected to IRQ0, used for system timer
 * - Channel 1: Historically used for RAM refresh (obsolete)
 * - Channel 2: Connected to PC speaker
 * 
 * We program Channel 0 to generate interrupts at 1000Hz (1ms intervals)
 * This gives us millisecond-precision timing for the OS
 */

#include "timer.h"
#include "io.h"
#include "serial.h"

/* PIT ports */
#define PIT_CHANNEL0 0x40   /* Channel 0 data port */
#define PIT_COMMAND  0x43   /* Command/Mode register */

/* PIT command bits */
#define PIT_CHANNEL0_SELECT 0x00  /* Select channel 0 */
#define PIT_ACCESS_LOHI    0x30   /* Access mode: lobyte/hibyte */
#define PIT_MODE_RATE_GEN  0x04   /* Mode 2: rate generator */

/* PIT frequency constants */
#define PIT_FREQUENCY 1193182  /* Base frequency of PIT in Hz */
#define TIMER_HZ 1000         /* Desired timer frequency (1000Hz = 1ms) */

/* IDT (Interrupt Descriptor Table) structures */
struct idt_entry {
    unsigned short base_low;   /* Lower 16 bits of handler address */
    unsigned short selector;   /* Kernel code segment selector */
    unsigned char zero;        /* Always 0 */
    unsigned char type_attr;   /* Type and attributes */
    unsigned short base_high;  /* Upper 16 bits of handler address */
} __attribute__((packed));

struct idt_ptr {
    unsigned short limit;      /* Size of IDT - 1 */
    unsigned int base;        /* Base address of IDT */
} __attribute__((packed));

/* IDT with 256 entries */
struct idt_entry idt[256];
struct idt_ptr idtp;

/* System tick counter (milliseconds since boot) */
static volatile unsigned int system_ticks = 0;

/* Assembly functions for interrupt handling */
extern void timer_interrupt_stub(void);
extern void default_interrupt_stub(void);
extern void load_idt(unsigned int);

/* Timer interrupt handler (called from assembly stub) */
void timer_handler(void) {
    /* Increment system tick counter */
    system_ticks++;
    
    /* Send EOI (End Of Interrupt) to PIC */
    outb(0x20, 0x20);
}

/* Default interrupt handler for unhandled interrupts */
void default_handler(unsigned int int_num) {
    /* Log unexpected interrupts for debugging */
    if (int_num < 32) {
        /* CPU exception - these are bad! */
        serial_write_string("EXCEPTION: ");
        serial_write_hex(int_num);
        serial_write_string(" - HALTING\n");
        /* Halt the CPU on exceptions */
        __asm__ __volatile__("cli; hlt");
    } else if (int_num >= 32 && int_num < 48) {
        /* Hardware interrupt (IRQ 0-15) */
        /* Send EOI to PIC */
        if (int_num >= 40) {
            /* IRQ 8-15: Send EOI to slave PIC */
            outb(0xA0, 0x20);
        }
        /* Always send EOI to master PIC */
        outb(0x20, 0x20);
    }
    /* Ignore other interrupts */
}

/* Set up an IDT entry */
static void idt_set_gate(unsigned char num, unsigned int base, 
                         unsigned short selector, unsigned char type_attr) {
    idt[num].base_low = base & 0xFFFF;
    idt[num].base_high = (base >> 16) & 0xFFFF;
    idt[num].selector = selector;
    idt[num].zero = 0;
    idt[num].type_attr = type_attr;
}

/* Initialize the IDT */
static void init_idt(void) {
    int i;
    
    /* Install default handler for ALL interrupts first */
    /* This prevents triple faults from unhandled interrupts */
    for (i = 0; i < 256; i++) {
        idt_set_gate(i, (unsigned int)default_interrupt_stub, 0x08, 0x8E);
    }
    
    /* Now install specific handlers */
    /* Timer interrupt handler at IRQ0 (interrupt 32) */
    idt_set_gate(32, (unsigned int)timer_interrupt_stub, 0x08, 0x8E);
    
    /* Set up IDT pointer */
    idtp.limit = (sizeof(struct idt_entry) * 256) - 1;
    idtp.base = (unsigned int)&idt;
    
    /* Load the IDT */
    __asm__ __volatile__("lidt %0" : : "m" (idtp));
}

/* Initialize PIC (Programmable Interrupt Controller) */
static void init_pic(void) {
    /* ICW1 - begin initialization */
    outb(0x20, 0x11);  /* Master PIC */
    outb(0xA0, 0x11);  /* Slave PIC */
    
    /* ICW2 - remap IRQs to interrupts 32-47 */
    outb(0x21, 0x20);  /* Master PIC vector offset (32) */
    outb(0xA1, 0x28);  /* Slave PIC vector offset (40) */
    
    /* ICW3 - setup cascading */
    outb(0x21, 0x04);  /* Tell master PIC that slave is at IRQ2 */
    outb(0xA1, 0x02);  /* Tell slave PIC its cascade identity */
    
    /* ICW4 - environment info */
    outb(0x21, 0x01);  /* 8086 mode */
    outb(0xA1, 0x01);
    
    /* Mask all interrupts except timer (IRQ0) and keyboard (IRQ1) */
    outb(0x21, 0xFC);  /* Master PIC: unmask IRQ0 and IRQ1 (0xFC = 11111100) */
    outb(0xA1, 0xFF);  /* Slave PIC: mask all */
}

/* Configure the PIT for desired frequency */
static void init_pit(void) {
    unsigned int divisor;
    
    /* Calculate divisor for desired frequency */
    divisor = PIT_FREQUENCY / TIMER_HZ;
    
    /* Send command to PIT */
    outb(PIT_COMMAND, PIT_CHANNEL0_SELECT | PIT_ACCESS_LOHI | PIT_MODE_RATE_GEN);
    
    /* Send divisor */
    outb(PIT_CHANNEL0, divisor & 0xFF);        /* Low byte */
    outb(PIT_CHANNEL0, (divisor >> 8) & 0xFF); /* High byte */
}

/* Initialize the timer system */
void init_timer(void) {
    /* Disable interrupts during setup */
    __asm__ __volatile__("cli");
    
    /* Initialize IDT */
    init_idt();
    
    /* Initialize PIC */
    init_pic();
    
    /* Initialize PIT */
    init_pit();
    
    /* Enable interrupts */
    __asm__ __volatile__("sti");
    
    serial_write_string("Timer initialized: 1000Hz (1ms ticks)\n");
}

/* Get current system ticks */
unsigned int get_ticks(void) {
    return system_ticks;
}

/* Get elapsed milliseconds since a previous tick count */
unsigned int get_elapsed_ms(unsigned int start_ticks) {
    return system_ticks - start_ticks;
}