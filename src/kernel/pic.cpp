#include "pic.h"

// Port I/O helpers
static inline void outb(uint16_t port, uint8_t val) {
    asm volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline void io_wait() {
    asm volatile("outb %%al, $0x80" : : "a"(0));
}

void PIC::initialize() {
    // Start initialization sequence (ICW1)
    outb(PIC1_CMD, ICW1_INIT);
    io_wait();
    outb(PIC2_CMD, ICW1_INIT);
    io_wait();
    
    // ICW2: Vector offsets
    // Map IRQ 0-7 to 32-39 (0x20-0x27)
    // Map IRQ 8-15 to 40-47 (0x28-0x2F)
    outb(PIC1_DATA, 0x20);  
    io_wait();
    outb(PIC2_DATA, 0x28);
    io_wait();
    
    // ICW3: Cascading
    outb(PIC1_DATA, 4);     // TC2 at IRQ2
    io_wait();
    outb(PIC2_DATA, 2);     // Cascade identity
    io_wait();
    
    // ICW4: 8086 mode
    outb(PIC1_DATA, ICW4_8086);
    io_wait();
    outb(PIC2_DATA, ICW4_8086);
    io_wait();
    
    // Unmask all interrupts EXCEPT Keyboard (IRQ1) to prevent hang
    // since we use polling for keyboard currently.
    outb(PIC1_DATA, 0x02); // Mask IRQ1 (Bit 1)
    outb(PIC2_DATA, 0x00);
}

void PIC::send_eoi(uint8_t irq) {
    if (irq >= 8) {
        outb(PIC2_CMD, 0x20);
    }
    outb(PIC1_CMD, 0x20);
}

void PIC::disable() {
    outb(PIC1_DATA, 0xFF);
    outb(PIC2_DATA, 0xFF);
}
