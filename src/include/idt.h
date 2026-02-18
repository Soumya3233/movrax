#ifndef IDT_H
#define IDT_H

#include "types.h"

// IDT entry structure
struct IDTEntry {
    uint16_t offset_low;   // Offset bits 0-15
    uint16_t selector;     // Code segment selector
    uint8_t  zero;         // Always zero
    uint8_t  type_attr;    // Type and attributes
    uint16_t offset_high;  // Offset bits 16-31
} __attribute__((packed));

// IDT pointer structure
struct IDTPointer {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

// IDT gate types
#define IDT_GATE_INTERRUPT 0x8E  // 32-bit interrupt gate
#define IDT_GATE_TRAP      0x8F  // 32-bit trap gate

// Exception numbers
#define EXCEPTION_DIVIDE_ERROR     0
#define EXCEPTION_DEBUG            1
#define EXCEPTION_NMI              2
#define EXCEPTION_BREAKPOINT       3
#define EXCEPTION_OVERFLOW         4
#define EXCEPTION_BOUND_RANGE      5
#define EXCEPTION_INVALID_OPCODE   6
#define EXCEPTION_NO_FPU           7
#define EXCEPTION_DOUBLE_FAULT     8
#define EXCEPTION_FPU_SEGMENT      9
#define EXCEPTION_INVALID_TSS      10
#define EXCEPTION_SEGMENT_NP       11
#define EXCEPTION_STACK_FAULT      12
#define EXCEPTION_GENERAL_PROT     13
#define EXCEPTION_PAGE_FAULT       14
#define EXCEPTION_FPU_ERROR        16

// IDT class
class IDT {
private:
    static IDTEntry entries[256];
    static IDTPointer pointer;
    
public:
    static void initialize();
    static void set_gate(uint8_t num, uint32_t handler, uint16_t selector, uint8_t type);
    static void load();
};

// Exception handler (called from assembly stubs)
extern "C" void exception_handler(uint32_t exception_num, uint32_t error_code, uint32_t eip);

#endif // IDT_H
