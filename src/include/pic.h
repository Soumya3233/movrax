#ifndef PIC_H
#define PIC_H

#include "types.h"

// PIC Ports
#define PIC1_CMD    0x20
#define PIC1_DATA   0x21
#define PIC2_CMD    0xA0
#define PIC2_DATA   0xA1

// Initialization commands
#define ICW1_INIT       0x11
#define ICW4_8086       0x01

// PIC class
class PIC {
public:
    static void initialize();
    static void send_eoi(uint8_t irq);
    static void disable();
};

#endif // PIC_H
