#ifndef PAGING_H
#define PAGING_H

#include "pmm.h"
#include "types.h"


// ============================================================================
// Page Table Entry Flags
// ============================================================================
#define PAGE_PRESENT 0x001      // Page is present in memory
#define PAGE_WRITABLE 0x002     // Page is writable
#define PAGE_USER 0x004         // Page is accessible from user mode
#define PAGE_WRITETHROUGH 0x008 // Write-through caching
#define PAGE_NOCACHE 0x010      // Disable caching
#define PAGE_ACCESSED 0x020     // Page has been accessed
#define PAGE_DIRTY 0x040        // Page has been written to
#define PAGE_SIZE_4MB 0x080     // 4MB page (for page directory only)
#define PAGE_GLOBAL 0x100       // Global page

// ============================================================================
// Security: W^X (Write XOR Execute) Policy
// On x86-32 without PAE/NX bit, we can't truly enforce no-execute.
// However, we CAN enforce:
//   - Kernel pages NOT accessible from user mode
//   - Writable pages clearly separated from read-only code pages
//   - Guard pages (unmapped) to catch stack overflows
// ============================================================================

// Convenience macros for security policies
#define PAGE_KERNEL_CODE (PAGE_PRESENT) // Read-only, kernel only
#define PAGE_KERNEL_DATA                                                       \
  (PAGE_PRESENT | PAGE_WRITABLE)                  // Read-write, kernel only
#define PAGE_USER_CODE (PAGE_PRESENT | PAGE_USER) // Read-only, user accessible
#define PAGE_USER_DATA                                                         \
  (PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER) // Read-write, user accessible
#define PAGE_GUARD 0 // No flags = unmapped = triggers page fault on access

// Page sizes
#define PAGE_ENTRIES 1024 // Entries per page table/directory
#define PAGE_TABLE_SIZE (PAGE_ENTRIES * sizeof(uint32_t))

// Virtual memory regions
#define PERSISTENT_REGION_VADDR 0xC0000000   // 3GB mark - persistent memory
#define PERSISTENT_REGION_SIZE (1024 * 1024) // 1MB persistent region

// Page directory entry (points to page table)
typedef uint32_t page_directory_entry_t;

// Page table entry (points to physical page)
typedef uint32_t page_table_entry_t;

// Paging manager class
class Paging {
private:
  page_directory_entry_t *page_directory;

  // Get page table for a virtual address, creating if necessary
  page_table_entry_t *get_page_table(uint32_t vaddr, bool create);

  // Internal: get page table without PAGE_USER on PDE (kernel-only tables)
  page_table_entry_t *get_kernel_page_table(uint32_t vaddr, bool create);

public:
  Paging();

  // Initialize paging structures
  void initialize();

  // Map a virtual address to a physical address
  void map_page(uint32_t vaddr, uint32_t paddr, uint32_t flags);

  // Unmap a virtual address
  void unmap_page(uint32_t vaddr);

  // Process isolation
  uint32_t create_address_space();
  void switch_directory(uint32_t dir_paddr);
  void map_page_in_dir(uint32_t dir_paddr, uint32_t vaddr, uint32_t paddr,
                       uint32_t flags);

  // Identity map a range (vaddr == paddr)
  void identity_map_range(uint32_t start, uint32_t end, uint32_t flags);

  // Map persistent memory region
  void map_persistent_region();

  // Enable paging
  void enable();

  // Get page directory physical address
  uint32_t get_page_directory_addr() { return (uint32_t)page_directory; }

  // Security: Guard page support
  void
  map_guard_page(uint32_t vaddr); // Creates unmapped page (catches overflow)

  // Security: Map stack with guard page below it
  void map_stack_with_guard(uint32_t stack_bottom_vaddr, uint32_t num_pages,
                            uint32_t flags);
  void map_stack_with_guard_in_dir(uint32_t dir_paddr,
                                   uint32_t stack_bottom_vaddr,
                                   uint32_t num_pages, uint32_t flags);
};

// Global paging instance
extern Paging paging;

// Assembly helpers for CR0/CR3
extern "C" void load_page_directory(uint32_t *dir);
extern "C" void enable_paging();

#endif // PAGING_H
