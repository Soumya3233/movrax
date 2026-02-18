#ifndef MULTIBOOT_H
#define MULTIBOOT_H

#include "types.h"

// Multiboot header magic values
#define MULTIBOOT_BOOTLOADER_MAGIC      0x2BADB002

// Multiboot info flags
#define MULTIBOOT_INFO_MEMORY           0x00000001
#define MULTIBOOT_INFO_BOOTDEV          0x00000002
#define MULTIBOOT_INFO_CMDLINE          0x00000004
#define MULTIBOOT_INFO_MODS             0x00000008
#define MULTIBOOT_INFO_MEM_MAP          0x00000040

// Memory map entry types
#define MULTIBOOT_MEMORY_AVAILABLE      1
#define MULTIBOOT_MEMORY_RESERVED       2
#define MULTIBOOT_MEMORY_ACPI_RECLAIMABLE 3
#define MULTIBOOT_MEMORY_NVS            4
#define MULTIBOOT_MEMORY_BADRAM         5

// Multiboot info structure passed by GRUB
struct MultibootInfo {
    uint32_t flags;
    uint32_t mem_lower;          // Amount of lower memory (KB)
    uint32_t mem_upper;          // Amount of upper memory (KB)
    uint32_t boot_device;
    uint32_t cmdline;
    uint32_t mods_count;
    uint32_t mods_addr;
    uint32_t syms[4];
    uint32_t mmap_length;        // Length of memory map
    uint32_t mmap_addr;          // Address of memory map
    uint32_t drives_length;
    uint32_t drives_addr;
    uint32_t config_table;
    uint32_t boot_loader_name;
    uint32_t apm_table;
    uint32_t vbe_control_info;
    uint32_t vbe_mode_info;
    uint16_t vbe_mode;
    uint16_t vbe_interface_seg;
    uint16_t vbe_interface_off;
    uint16_t vbe_interface_len;
} __attribute__((packed));

// Memory map entry structure
struct MultibootMmapEntry {
    uint32_t size;               // Size of this entry (excluding size field)
    uint64_t addr;               // Start address
    uint64_t len;                // Length in bytes
    uint32_t type;               // Type (1 = available, others = reserved)
} __attribute__((packed));

#endif // MULTIBOOT_H
