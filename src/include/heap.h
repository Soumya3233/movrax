#ifndef HEAP_H
#define HEAP_H

#include "types.h"

// ============================================================================
// MOVRAX Kernel Heap Allocator
// Simple block-based allocator for dynamic kernel memory
// Deterministic allocation for real-time systems
// ============================================================================

#define HEAP_START 0xD0000000         // Virtual address for kernel heap
#define HEAP_INITIAL_SIZE (64 * 4096) // 256KB initial heap
#define HEAP_MAX_SIZE (1024 * 4096)   // 4MB maximum
#define HEAP_MAGIC 0xCAFEBEEF
#define HEAP_MIN_BLOCK 32

// Block header for free list
struct HeapBlock {
  uint32_t magic;  // HEAP_MAGIC — detects corruption
  uint32_t size;   // Size of usable area (excluding header)
  bool free;       // Is this block free?
  HeapBlock *next; // Next block in list
  HeapBlock *prev; // Previous block in list
};

class KernelHeap {
private:
  HeapBlock *head; // Head of block list
  uint32_t heap_start;
  uint32_t heap_end; // Current end of mapped heap
  uint32_t heap_max; // Maximum heap can grow to

  // Split a free block if it's large enough
  void split_block(HeapBlock *block, uint32_t size);

  // Merge adjacent free blocks
  void merge_free();

  // Expand heap by mapping more pages
  bool expand(uint32_t min_size);

  // Validate block header integrity
  bool validate_block(HeapBlock *block);

public:
  // Initialize heap with initial size
  void initialize();

  // Allocate memory
  void *alloc(uint32_t size);

  // Free memory
  void free(void *ptr);

  // Get heap statistics
  uint32_t get_used();
  uint32_t get_free();
  uint32_t get_total() { return heap_end - heap_start; }
};

// Global heap instance
extern KernelHeap kheap;

// C-style wrappers
void *kmalloc(uint32_t size);
void kfree(void *ptr);

#endif // HEAP_H
