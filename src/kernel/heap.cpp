#include "heap.h"
#include "paging.h"
#include "pmm.h"
#include "vga.h"

// ============================================================================
// MOVRAX Kernel Heap Allocator Implementation
// First-fit block allocator with coalescing
// Deterministic: O(n) allocation, O(1) free with lazy merge
// ============================================================================

KernelHeap kheap;

bool KernelHeap::validate_block(HeapBlock *block) {
  if (block->magic != HEAP_MAGIC) {
    terminal.set_color(VGA_COLOR_WHITE, VGA_COLOR_RED);
    terminal.write_string("[HEAP] CORRUPTION DETECTED!\n");
    terminal.set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    return false;
  }
  return true;
}

void KernelHeap::initialize() {
  heap_start = HEAP_START;
  heap_end = HEAP_START;
  heap_max = HEAP_START + HEAP_MAX_SIZE;
  head = nullptr;

  // Map initial heap pages
  uint32_t pages = HEAP_INITIAL_SIZE / PAGE_SIZE;
  for (uint32_t i = 0; i < pages; i++) {
    void *frame = pmm.alloc_frame();
    if (!frame) {
      terminal.set_color(VGA_COLOR_WHITE, VGA_COLOR_RED);
      terminal.write_string("[HEAP] Failed to allocate initial pages\n");
      terminal.set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
      return;
    }
    paging.map_page(heap_end, (uint32_t)frame, PAGE_KERNEL_DATA);
    heap_end += PAGE_SIZE;
  }

  // Create initial free block spanning entire heap
  head = (HeapBlock *)heap_start;
  head->magic = HEAP_MAGIC;
  head->size = (heap_end - heap_start) - sizeof(HeapBlock);
  head->free = true;
  head->next = nullptr;
  head->prev = nullptr;
}

bool KernelHeap::expand(uint32_t min_size) {
  // Calculate pages needed
  uint32_t pages = (min_size + PAGE_SIZE - 1) / PAGE_SIZE;
  if (pages < 4)
    pages = 4; // Minimum 16KB expansion

  uint32_t new_end = heap_end + (pages * PAGE_SIZE);
  if (new_end > heap_max)
    return false;

  // Map new pages
  for (uint32_t addr = heap_end; addr < new_end; addr += PAGE_SIZE) {
    void *frame = pmm.alloc_frame();
    if (!frame)
      return false;
    paging.map_page(addr, (uint32_t)frame, PAGE_KERNEL_DATA);
  }

  // Find last block and extend it, or create new block
  HeapBlock *last = head;
  while (last && last->next)
    last = last->next;

  if (last && last->free) {
    // Extend the last free block
    last->size += (new_end - heap_end);
  } else {
    // Create new free block at end
    HeapBlock *new_block = (HeapBlock *)heap_end;
    new_block->magic = HEAP_MAGIC;
    new_block->size = (new_end - heap_end) - sizeof(HeapBlock);
    new_block->free = true;
    new_block->next = nullptr;
    new_block->prev = last;
    if (last)
      last->next = new_block;
  }

  heap_end = new_end;
  return true;
}

void KernelHeap::split_block(HeapBlock *block, uint32_t size) {
  // Only split if remaining space is large enough for header + min payload
  uint32_t remaining = block->size - size - sizeof(HeapBlock);
  if (remaining < HEAP_MIN_BLOCK)
    return;

  // Create new free block after allocated block
  HeapBlock *new_block =
      (HeapBlock *)((uint8_t *)block + sizeof(HeapBlock) + size);
  new_block->magic = HEAP_MAGIC;
  new_block->size = remaining;
  new_block->free = true;
  new_block->next = block->next;
  new_block->prev = block;

  if (block->next)
    block->next->prev = new_block;
  block->next = new_block;
  block->size = size;
}

void KernelHeap::merge_free() {
  HeapBlock *current = head;
  while (current && current->next) {
    if (!validate_block(current))
      return;

    if (current->free && current->next->free) {
      // Merge current with next
      current->size += sizeof(HeapBlock) + current->next->size;
      current->next = current->next->next;
      if (current->next)
        current->next->prev = current;
      // Don't advance — check if we can merge again
    } else {
      current = current->next;
    }
  }
}

void *KernelHeap::alloc(uint32_t size) {
  if (size == 0)
    return nullptr;

  // Align to 8 bytes for safety
  size = (size + 7) & ~7;

  // First-fit search
  HeapBlock *current = head;
  while (current) {
    if (!validate_block(current))
      return nullptr;

    if (current->free && current->size >= size) {
      // Found a suitable block
      split_block(current, size);
      current->free = false;
      return (void *)((uint8_t *)current + sizeof(HeapBlock));
    }
    current = current->next;
  }

  // No suitable block found — try to expand
  if (expand(size + sizeof(HeapBlock))) {
    return alloc(size); // Retry after expansion
  }

  terminal.set_color(VGA_COLOR_WHITE, VGA_COLOR_RED);
  terminal.write_string("[HEAP] Allocation failed: out of memory\n");
  terminal.set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
  return nullptr;
}

void KernelHeap::free(void *ptr) {
  if (!ptr)
    return;

  // Validate pointer is within heap range
  uint32_t addr = (uint32_t)ptr;
  if (addr < heap_start || addr >= heap_end) {
    terminal.set_color(VGA_COLOR_WHITE, VGA_COLOR_RED);
    terminal.write_string("[HEAP] Invalid free: pointer outside heap\n");
    terminal.set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    return;
  }

  // Get block header
  HeapBlock *block = (HeapBlock *)((uint8_t *)ptr - sizeof(HeapBlock));

  if (!validate_block(block))
    return;

  if (block->free) {
    terminal.set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
    terminal.write_string("[HEAP] Double-free detected!\n");
    terminal.set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    return;
  }

  block->free = true;

  // Merge adjacent free blocks
  merge_free();
}

uint32_t KernelHeap::get_used() {
  uint32_t used = 0;
  HeapBlock *current = head;
  while (current) {
    if (!current->free)
      used += current->size + sizeof(HeapBlock);
    current = current->next;
  }
  return used;
}

uint32_t KernelHeap::get_free() {
  uint32_t free_size = 0;
  HeapBlock *current = head;
  while (current) {
    if (current->free)
      free_size += current->size;
    current = current->next;
  }
  return free_size;
}

// C-style wrapper functions
void *kmalloc(uint32_t size) { return kheap.alloc(size); }

void kfree(void *ptr) { kheap.free(ptr); }
