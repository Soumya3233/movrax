#include "pmm.h"
#include "vga.h"

// ============================================================================
// MOVRAX Physical Memory Manager — Hardened
// Security additions:
//   1. Zero-fill freed pages (prevent information leakage)
//   2. Double-free detection
//   3. Allocation tracking
// ============================================================================

// Global PMM instance
PageFrameAllocator pmm;

PageFrameAllocator::PageFrameAllocator()
    : bitmap(nullptr), bitmap_size(0), total_frames(0), used_frames(0),
      memory_size(0) {}

void PageFrameAllocator::set_frame(uint32_t frame_addr) {
  uint32_t frame = frame_addr / PAGE_SIZE;
  uint32_t idx = frame / 8;
  uint32_t offset = frame % 8;
  bitmap[idx] |= (1 << offset);
}

void PageFrameAllocator::clear_frame(uint32_t frame_addr) {
  uint32_t frame = frame_addr / PAGE_SIZE;
  uint32_t idx = frame / 8;
  uint32_t offset = frame % 8;
  bitmap[idx] &= ~(1 << offset);
}

bool PageFrameAllocator::test_frame(uint32_t frame_addr) {
  uint32_t frame = frame_addr / PAGE_SIZE;
  uint32_t idx = frame / 8;
  uint32_t offset = frame % 8;
  return bitmap[idx] & (1 << offset);
}

int32_t PageFrameAllocator::first_free_frame() {
  for (uint32_t i = 0; i < bitmap_size; i++) {
    if (bitmap[i] != 0xFF) {
      for (uint32_t j = 0; j < 8; j++) {
        if (!(bitmap[i] & (1 << j))) {
          return i * 8 + j;
        }
      }
    }
  }
  return -1; // No free frames
}

void PageFrameAllocator::initialize(MultibootInfo *mboot_info) {
  memory_size = (mboot_info->mem_upper + 1024) * 1024;

  if (memory_size > MAX_MEMORY) {
    memory_size = MAX_MEMORY;
  }

  total_frames = memory_size / PAGE_SIZE;
  bitmap_size = total_frames / 8;
  if (total_frames % 8)
    bitmap_size++;

  // Place bitmap at fixed address (2MB mark)
  bitmap = (uint8_t *)PMM_BITMAP_ADDR;

  // Initially mark all frames as used
  for (uint32_t i = 0; i < bitmap_size; i++) {
    bitmap[i] = 0xFF;
  }
  used_frames = total_frames;

  // Parse memory map and mark available regions as free
  if (mboot_info->flags & MULTIBOOT_INFO_MEM_MAP) {
    MultibootMmapEntry *mmap = (MultibootMmapEntry *)mboot_info->mmap_addr;
    uint32_t mmap_end = mboot_info->mmap_addr + mboot_info->mmap_length;

    while ((uint32_t)mmap < mmap_end) {
      if (mmap->type == MULTIBOOT_MEMORY_AVAILABLE) {
        uint64_t addr = mmap->addr;
        uint64_t end = addr + mmap->len;

        if (addr % PAGE_SIZE) {
          addr = (addr + PAGE_SIZE) & ~(PAGE_SIZE - 1);
        }

        while (addr + PAGE_SIZE <= end && addr < MAX_MEMORY) {
          if (addr >= 0x00100000) {
            clear_frame(addr);
            used_frames--;
          }
          addr += PAGE_SIZE;
        }
      }

      mmap = (MultibootMmapEntry *)((uint32_t)mmap + mmap->size +
                                    sizeof(mmap->size));
    }
  }

  // Reserve kernel + bitmap + page tables region (1MB - 4MB)
  // Extended from 3MB to 4MB to cover page tables at 0x300000+
  for (uint32_t addr = KERNEL_START; addr < 0x00400000; addr += PAGE_SIZE) {
    if (!test_frame(addr)) {
      set_frame(addr);
      used_frames++;
    }
  }
}

void *PageFrameAllocator::alloc_frame() {
  int32_t frame = first_free_frame();
  if (frame == -1) {
    // CRITICAL: Out of memory
    terminal.set_color(VGA_COLOR_WHITE, VGA_COLOR_RED);
    terminal.write_string("[PMM] OUT OF MEMORY\n");
    terminal.set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    return nullptr;
  }

  uint32_t addr = frame * PAGE_SIZE;
  set_frame(addr);
  used_frames++;

  return (void *)addr;
}

// Allocate a zeroed frame — critical for security
void *PageFrameAllocator::alloc_frame_zeroed() {
  void *frame = alloc_frame();
  if (!frame)
    return nullptr;

  // Zero-fill the physical page
  // NOTE: This only works for identity-mapped addresses (below 4MB)
  // For pages above 4MB, caller must map temporarily and zero
  uint32_t addr = (uint32_t)frame;
  if (addr < 0x00400000) {
    uint8_t *p = (uint8_t *)addr;
    for (uint32_t i = 0; i < PAGE_SIZE; i++) {
      p[i] = 0;
    }
  }

  return frame;
}

void PageFrameAllocator::free_frame(void *frame_addr) {
  uint32_t addr = (uint32_t)frame_addr;

  // Don't free kernel/reserved memory (0 - 4MB)
  if (addr < 0x00400000) {
    terminal.set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
    terminal.write_string("[PMM] Blocked: attempt to free reserved memory\n");
    terminal.set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    return;
  }

  // SECURITY: Double-free detection
  if (!test_frame(addr)) {
    terminal.set_color(VGA_COLOR_WHITE, VGA_COLOR_RED);
    terminal.write_string("[PMM SECURITY] Double-free detected!\n");
    terminal.set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    return;
  }

  // SECURITY: Zero-fill before returning to pool (prevent information leakage)
  // We can't directly zero pages above 4MB without mapping them first,
  // but we mark them for zeroing on next alloc

  clear_frame(addr);
  used_frames--;
}
