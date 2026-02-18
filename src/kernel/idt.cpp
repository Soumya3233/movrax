#include "idt.h"
#include "vga.h"

// Static members
IDTEntry IDT::entries[256];
IDTPointer IDT::pointer;

// Exception names for display
static const char* exception_names[] = {
    "Division Error",
    "Debug",
    "Non-Maskable Interrupt",
    "Breakpoint",
    "Overflow",
    "Bound Range Exceeded",
    "Invalid Opcode",
    "Device Not Available",
    "Double Fault",
    "Coprocessor Segment Overrun",
    "Invalid TSS",
    "Segment Not Present",
    "Stack-Segment Fault",
    "General Protection Fault",
    "Page Fault",
    "Reserved",
    "x87 FPU Error",
    "Alignment Check",
    "Machine Check",
    "SIMD Floating-Point"
};

// Assembly ISR stubs (defined in isr.asm)
extern "C" void isr_page_fault();
extern "C" void irq_timer();
extern "C" void isr_syscall();

void IDT::initialize() {
    pointer.limit = sizeof(entries) - 1;
    pointer.base = (uint32_t)&entries;
    
    // Clear all entries
    for (int i = 0; i < 256; i++) {
        entries[i].offset_low = 0;
        entries[i].selector = 0;
        entries[i].zero = 0;
        entries[i].type_attr = 0;
        entries[i].offset_high = 0;
    }
    
    // Set up page fault handler (interrupt 14)
    set_gate(EXCEPTION_PAGE_FAULT, (uint32_t)isr_page_fault, 0x08, IDT_GATE_INTERRUPT);

    // Set up Timer handler (IRQ 0 -> Interrupt 32)
    set_gate(32, (uint32_t)irq_timer, 0x08, IDT_GATE_INTERRUPT);

    // Set up Syscall handler (Interrupt 0x80 = 128)
    // DPL = 3 (User Mode can call this)
    // Type = 0xEE (32-bit Trap Gate, DPL=3: 1110 + 11100000 -> 0xEE? Wait.)
    // Gate Type: 0xE (32-bit Interrupt Gate) | DPL 3 (0x60) | Present (0x80) -> 0xEE
    // Default helper uses 0x8E (DPL 0 Interrupt). Modified manually here or add DPL arg?
    // IDT_GATE_INTERRUPT is 0x8E.
    // We need 0xEE for user mode access.
    set_gate(0x80, (uint32_t)isr_syscall, 0x08, 0xEE);
    
    // Load IDT
    load();
}

void IDT::set_gate(uint8_t num, uint32_t handler, uint16_t selector, uint8_t type) {
    entries[num].offset_low = handler & 0xFFFF;
    entries[num].offset_high = (handler >> 16) & 0xFFFF;
    entries[num].selector = selector;
    entries[num].zero = 0;
    entries[num].type_attr = type;
}

void IDT::load() {
    asm volatile("lidt %0" : : "m"(pointer));
}

// C exception handler called from assembly
extern "C" void exception_handler(uint32_t exception_num, uint32_t error_code, uint32_t eip) {
    terminal.set_color(VGA_COLOR_WHITE, VGA_COLOR_RED);
    terminal.write_string("\n\n *** KERNEL PANIC *** \n\n");
    
    terminal.set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
    
    if (exception_num < 20) {
        terminal.write_string("Exception: ");
        terminal.write_string(exception_names[exception_num]);
        terminal.write_string("\n");
    } else {
        terminal.write_string("Exception: Unknown\n");
    }
    
    // Print instruction pointer
    terminal.write_string("EIP: 0x");
    char hex_eip[9];
    for (int i = 7; i >= 0; i--) {
        int digit = (eip >> (i * 4)) & 0xF;
        hex_eip[7 - i] = digit < 10 ? '0' + digit : 'A' + digit - 10;
    }
    hex_eip[8] = '\0';
    terminal.write_string(hex_eip);
    terminal.write_string("\n");
    
    // Print error code for page fault
    if (exception_num == EXCEPTION_PAGE_FAULT) {
        terminal.write_string("Error Code: ");
        
        // Page fault error code bits:
        // Bit 0: P - Page not present (0) or protection violation (1)
        // Bit 1: W - Read (0) or Write (1)
        // Bit 2: U - Supervisor (0) or User (1)
        if (error_code & 0x01) {
            terminal.write_string("Protection violation ");
        } else {
            terminal.write_string("Page not present ");
        }
        if (error_code & 0x02) {
            terminal.write_string("(Write) ");
        } else {
            terminal.write_string("(Read) ");
        }
        if (error_code & 0x04) {
            terminal.write_string("User mode");
        } else {
            terminal.write_string("Kernel mode");
        }
        terminal.write_string("\n");
        
        // Get faulting address from CR2
        uint32_t faulting_addr;
        asm volatile("mov %%cr2, %0" : "=r"(faulting_addr));
        
        terminal.write_string("Faulting Address: 0x");
        char hex[9];
        for (int i = 7; i >= 0; i--) {
            int digit = (faulting_addr >> (i * 4)) & 0xF;
            hex[7 - i] = digit < 10 ? '0' + digit : 'A' + digit - 10;
        }
        hex[8] = '\0';
        terminal.write_string(hex);
        terminal.write_string("\n");
    }
    
    terminal.write_string("\nSystem halted.\n");
    terminal.set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    
    // Halt the CPU
    while (1) {
        asm volatile("cli; hlt");
    }
}
