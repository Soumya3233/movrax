#include "gdt.h"

// Static members
GDTEntry GDT::entries[6];
GDTPointer GDT::pointer;
TSSEntry GDT::tss;

void GDT::initialize() {
    pointer.limit = (sizeof(GDTEntry) * 6) - 1;
    pointer.base = (uint32_t)&entries;
    
    // Null segment
    set_gate(0, 0, 0, 0, 0);
    
    // Kernel Code Segment (0x08)
    // Base=0, Limit=4GB, Access=0x9A (Pr, Ring0, Ex, Rd), Gran=0xCF (4KB)
    set_gate(1, 0, 0xFFFFFFFF, 0x9A, 0xCF);
    
    // Kernel Data Segment (0x10)
    // Base=0, Limit=4GB, Access=0x92 (Pr, Ring0, RW), Gran=0xCF
    set_gate(2, 0, 0xFFFFFFFF, 0x92, 0xCF);
    
    // User Code Segment (0x18)
    // Base=0, Limit=4GB, Access=0xFA (Pr, Ring3, Ex, Rd), Gran=0xCF
    set_gate(3, 0, 0xFFFFFFFF, 0xFA, 0xCF);
    
    // User Data Segment (0x20)
    // Base=0, Limit=4GB, Access=0xF2 (Pr, Ring3, RW), Gran=0xCF
    set_gate(4, 0, 0xFFFFFFFF, 0xF2, 0xCF);
    
    // TSS Segment (0x28)
    // Base=&tss, Limit=sizeof(tss), Access=0x89 (Pr, Ring0, TSS), Gran=0x00
    uint32_t tss_base = (uint32_t)&tss;
    uint32_t tss_limit = sizeof(tss);
    set_gate(5, tss_base, tss_limit, 0xE9, 0x00);
    
    // Initialize TSS
    // We only need to set ss0 and esp0 for interrupts coming from user mode
    for (uint32_t i = 0; i < sizeof(tss); i++) {
        ((uint8_t*)&tss)[i] = 0;
    }
    tss.ss0 = 0x10;  // Kernel Data Segment
    tss.esp0 = 0;    // Will be set by scheduler
    tss.cs = 0x08 | 3;
    tss.ss = 0x10 | 3;
    tss.ds = 0x10 | 3;
    tss.es = 0x10 | 3;
    tss.fs = 0x10 | 3;
    tss.gs = 0x10 | 3;
    tss.iomap_base = sizeof(tss);
    
    // Load GDT
    gdt_flush((uint32_t)&pointer);
    
    // Load TSS
    tss_flush();
}

void GDT::set_gate(int32_t num, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran) {
    entries[num].base_low = (base & 0xFFFF);
    entries[num].base_middle = (base >> 16) & 0xFF;
    entries[num].base_high = (base >> 24) & 0xFF;
    
    entries[num].limit_low = (limit & 0xFFFF);
    entries[num].granularity = (limit >> 16) & 0x0F;
    entries[num].granularity |= (gran & 0xF0);
    entries[num].access = access;
}

void GDT::set_tss_stack(uint32_t kernel_esp) {
    tss.esp0 = kernel_esp;
}
