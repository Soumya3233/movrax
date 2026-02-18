#include "paging.h"
#include "vga.h"

// ============================================================================
// MOVRAX Military-Grade Paging Implementation
// Key security changes:
//   1. Kernel identity map NO LONGER has PAGE_USER flag
//   2. Guard pages for stack overflow detection
//   3. W^X policy enforcement via flag macros
// ============================================================================

// Global paging instance
Paging paging;

// Pre-allocated page structures at fixed addresses
#define PAGE_DIR_ADDR 0x00300000
#define PAGE_TABLES_START 0x00301000

// Keep track of next available page table slot
static uint32_t next_page_table = PAGE_TABLES_START;

Paging::Paging() : page_directory(nullptr) {}

void Paging::initialize() {
  page_directory = (page_directory_entry_t *)PAGE_DIR_ADDR;

  // Clear page directory
  for (int i = 0; i < PAGE_ENTRIES; i++) {
    page_directory[i] = 0;
  }

  // ========================================================================
  // SECURITY FIX: Identity map first 4MB as KERNEL-ONLY
  // OLD (vulnerable): PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER
  // NEW (secure):     PAGE_PRESENT | PAGE_WRITABLE (no PAGE_USER)
  //
  // This means user-mode processes CANNOT access kernel memory.
  // Any user-mode access to 0x00000000-0x003FFFFF will trigger a page fault.
  // ========================================================================
  identity_map_range(0x00000000, 0x00400000, PAGE_KERNEL_DATA);

  // VGA memory (0xB8000) is within first 4MB, already covered.
  // User processes that need VGA access must go through syscalls.

  // Map persistent region at 0xC0000000
  map_persistent_region();
}

// Kernel-only page table: PDE does NOT have PAGE_USER
page_table_entry_t *Paging::get_kernel_page_table(uint32_t vaddr, bool create) {
  uint32_t pd_index = vaddr >> 22;

  if (page_directory[pd_index] & PAGE_PRESENT) {
    return (page_table_entry_t *)(page_directory[pd_index] & 0xFFFFF000);
  }

  if (!create)
    return nullptr;

  page_table_entry_t *page_table = (page_table_entry_t *)next_page_table;
  next_page_table += PAGE_SIZE;

  for (int i = 0; i < PAGE_ENTRIES; i++) {
    page_table[i] = 0;
  }

  // KERNEL ONLY: No PAGE_USER on the PDE
  page_directory[pd_index] =
      (uint32_t)page_table | PAGE_PRESENT | PAGE_WRITABLE;

  return page_table;
}

page_table_entry_t *Paging::get_page_table(uint32_t vaddr, bool create) {
  uint32_t pd_index = vaddr >> 22;

  if (page_directory[pd_index] & PAGE_PRESENT) {
    return (page_table_entry_t *)(page_directory[pd_index] & 0xFFFFF000);
  }

  if (!create)
    return nullptr;

  page_table_entry_t *page_table = (page_table_entry_t *)next_page_table;
  next_page_table += PAGE_SIZE;

  for (int i = 0; i < PAGE_ENTRIES; i++) {
    page_table[i] = 0;
  }

  // User-accessible PDE (needed for user-space mappings)
  page_directory[pd_index] =
      (uint32_t)page_table | PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER;

  return page_table;
}

void Paging::map_page(uint32_t vaddr, uint32_t paddr, uint32_t flags) {
  // Determine if this is a kernel or user mapping
  page_table_entry_t *page_table;

  if (flags & PAGE_USER) {
    page_table = get_page_table(vaddr, true);
  } else {
    page_table = get_kernel_page_table(vaddr, true);
  }

  if (!page_table)
    return;

  uint32_t pt_index = (vaddr >> 12) & 0x3FF;
  page_table[pt_index] = (paddr & 0xFFFFF000) | (flags & 0xFFF);
}

void Paging::unmap_page(uint32_t vaddr) {
  // Try both kernel and user page tables
  uint32_t pd_index = vaddr >> 22;

  if (!(page_directory[pd_index] & PAGE_PRESENT))
    return;

  page_table_entry_t *page_table =
      (page_table_entry_t *)(page_directory[pd_index] & 0xFFFFF000);
  uint32_t pt_index = (vaddr >> 12) & 0x3FF;
  page_table[pt_index] = 0;

  // Invalidate TLB entry
  asm volatile("invlpg (%0)" : : "r"(vaddr) : "memory");
}

void Paging::identity_map_range(uint32_t start, uint32_t end, uint32_t flags) {
  start &= 0xFFFFF000;
  end = (end + PAGE_SIZE - 1) & 0xFFFFF000;

  for (uint32_t addr = start; addr < end; addr += PAGE_SIZE) {
    map_page(addr, addr, flags);
  }
}

void Paging::map_persistent_region() {
  uint32_t vaddr = PERSISTENT_REGION_VADDR;
  uint32_t paddr = 0x00400000; // Physical frames starting at 4MB
  uint32_t pages_needed = PERSISTENT_REGION_SIZE / PAGE_SIZE;

  for (uint32_t i = 0; i < pages_needed; i++) {
    // Persistent region: kernel-only writable (user accesses through syscalls)
    map_page(vaddr, paddr, PAGE_KERNEL_DATA);
    vaddr += PAGE_SIZE;
    paddr += PAGE_SIZE;
  }
}

void Paging::enable() {
  // Load page directory into CR3
  asm volatile("mov %0, %%cr3" : : "r"(page_directory));

  // Enable paging in CR0
  uint32_t cr0;
  asm volatile("mov %%cr0, %0" : "=r"(cr0));
  cr0 |= 0x80000000; // Set PG bit
  // Also enable Write Protect (WP) bit to enforce read-only pages in kernel
  // mode
  cr0 |= 0x00010000; // Set WP bit
  asm volatile("mov %0, %%cr0" : : "r"(cr0));
}

// ============================================================================
// Guard Pages: Unmapped page that catches stack overflow via page fault
// ============================================================================

void Paging::map_guard_page(uint32_t vaddr) {
  // Explicitly unmap (set to 0) — any access triggers page fault
  uint32_t pd_index = vaddr >> 22;

  if (page_directory[pd_index] & PAGE_PRESENT) {
    page_table_entry_t *page_table =
        (page_table_entry_t *)(page_directory[pd_index] & 0xFFFFF000);
    uint32_t pt_index = (vaddr >> 12) & 0x3FF;
    page_table[pt_index] = PAGE_GUARD; // 0 = not present
  }
  // If PDE doesn't exist, page is already unmapped = guard page by default
}

void Paging::map_stack_with_guard(uint32_t stack_bottom_vaddr,
                                  uint32_t num_pages, uint32_t flags) {
  // Guard page at the bottom (lowest address)
  map_guard_page(stack_bottom_vaddr);

  // Stack pages above the guard
  for (uint32_t i = 1; i <= num_pages; i++) {
    uint32_t vaddr = stack_bottom_vaddr + (i * PAGE_SIZE);
    void *phys = pmm.alloc_frame();
    if (phys) {
      map_page(vaddr, (uint32_t)phys, flags);
    }
  }
}

void Paging::map_stack_with_guard_in_dir(uint32_t dir_paddr,
                                         uint32_t stack_bottom_vaddr,
                                         uint32_t num_pages, uint32_t flags) {
  // Guard page (don't map anything = page fault on access)
  // By default pages are not present, so just skip mapping the guard page

  // Stack pages above the guard
  for (uint32_t i = 1; i <= num_pages; i++) {
    uint32_t vaddr = stack_bottom_vaddr + (i * PAGE_SIZE);
    void *phys = pmm.alloc_frame();
    if (phys) {
      map_page_in_dir(dir_paddr, vaddr, (uint32_t)phys, flags);
    }
  }
}

// ============================================================================
// Process Address Space Creation
// ============================================================================

uint32_t Paging::create_address_space() {
  void *p_addr = pmm.alloc_frame();
  if (!p_addr)
    return 0;

  uint32_t dir_paddr = (uint32_t)p_addr;

  uint32_t temp_vaddr = 0xFFFFF000;
  map_page(temp_vaddr, dir_paddr, PAGE_PRESENT | PAGE_WRITABLE);

  page_directory_entry_t *new_dir = (page_directory_entry_t *)temp_vaddr;

  // Clear directory
  for (int i = 0; i < PAGE_ENTRIES; i++) {
    new_dir[i] = 0;
  }

  // Copy Kernel Mappings (Identity Map 0-4MB) — kernel-only, NOT user
  new_dir[0] = page_directory[0];

  // Copy persistent region mapping (0xC0000000 → index 768)
  uint32_t persistent_idx = PERSISTENT_REGION_VADDR >> 22;
  new_dir[persistent_idx] = page_directory[persistent_idx];

  // Copy any additional kernel page tables (high addresses used for temp maps)
  // Pages 0xFFFF_x000 are in PDE index 1023
  if (page_directory[1023] & PAGE_PRESENT) {
    new_dir[1023] = page_directory[1023];
  }

  unmap_page(temp_vaddr);

  return dir_paddr;
}

void Paging::switch_directory(uint32_t dir_paddr) {
  asm volatile("mov %0, %%cr3" : : "r"(dir_paddr));
}

void Paging::map_page_in_dir(uint32_t dir_paddr, uint32_t vaddr, uint32_t paddr,
                             uint32_t flags) {
  uint32_t temp_dir_vaddr = 0xFFFFF000;
  map_page(temp_dir_vaddr, dir_paddr, PAGE_PRESENT | PAGE_WRITABLE);

  page_directory_entry_t *dir = (page_directory_entry_t *)temp_dir_vaddr;
  uint32_t pd_index = vaddr >> 22;

  uint32_t table_paddr = 0;

  if (dir[pd_index] & PAGE_PRESENT) {
    table_paddr = dir[pd_index] & 0xFFFFF000;
  } else {
    void *p = pmm.alloc_frame();
    if (!p)
      return;
    table_paddr = (uint32_t)p;

    // Map table temporarily to clear it
    uint32_t temp_table_vaddr = 0xFFFFE000;
    map_page(temp_table_vaddr, table_paddr, PAGE_PRESENT | PAGE_WRITABLE);
    page_table_entry_t *t = (page_table_entry_t *)temp_table_vaddr;
    for (int i = 0; i < PAGE_ENTRIES; i++)
      t[i] = 0;
    unmap_page(temp_table_vaddr);

    // Set PDE flags based on whether this is user or kernel mapping
    uint32_t pde_flags = PAGE_PRESENT | PAGE_WRITABLE;
    if (flags & PAGE_USER)
      pde_flags |= PAGE_USER;
    dir[pd_index] = table_paddr | pde_flags;
  }

  // Map table to edit entry
  uint32_t temp_table_vaddr = 0xFFFFE000;
  map_page(temp_table_vaddr, table_paddr, PAGE_PRESENT | PAGE_WRITABLE);

  page_table_entry_t *table = (page_table_entry_t *)temp_table_vaddr;
  uint32_t pt_index = (vaddr >> 12) & 0x3FF;
  table[pt_index] = (paddr & 0xFFFFF000) | (flags & 0xFFF);

  unmap_page(temp_table_vaddr);
  unmap_page(temp_dir_vaddr);
}
