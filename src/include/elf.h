#ifndef ELF_H
#define ELF_H

#include "types.h"

// ELF File Header
#define EI_NIDENT 16

struct Elf32_Ehdr {
    unsigned char e_ident[EI_NIDENT];
    uint16_t      e_type;
    uint16_t      e_machine;
    uint32_t      e_version;
    uint32_t      e_entry;      // Entry point address
    uint32_t      e_phoff;      // Program Header Offset
    uint32_t      e_shoff;      // Section Header Offset
    uint32_t      e_flags;
    uint16_t      e_ehsize;
    uint16_t      e_phentsize;  // Size of one Program Header
    uint16_t      e_phnum;      // Number of Program Headers
    uint16_t      e_shentsize;
    uint16_t      e_shnum;
    uint16_t      e_shstrndx;
};

// Program Header
struct Elf32_Phdr {
    uint32_t p_type;
    uint32_t p_offset;      // Offset in file
    uint32_t p_vaddr;       // Virtual address in memory
    uint32_t p_paddr;
    uint32_t p_filesz;      // Size in file
    uint32_t p_memsz;       // Size in memory
    uint32_t p_flags;
    uint32_t p_align;
};

class ELF {
public:
    static int load(const char* path);
};

#endif // ELF_H
