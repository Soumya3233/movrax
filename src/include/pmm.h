#ifndef PMM_H
#define PMM_H

#include "multiboot.h"
#include "types.h"


// Page size: 4KB
#define PAGE_SIZE 4096

// Memory regions
#define KERNEL_START 0x00100000    // 1MB - where kernel is loaded
#define PMM_BITMAP_ADDR 0x00200000 // 2MB - where we store bitmap

// Maximum physical memory we support (128 MB)
#define MAX_MEMORY (128 * 1024 * 1024)
#define MAX_FRAMES (MAX_MEMORY / PAGE_SIZE)

// Physical Memory Manager
class PageFrameAllocator {
private:
  uint8_t *bitmap;       // Bitmap: 1 bit per frame (1 = used, 0 = free)
  uint32_t bitmap_size;  // Size of bitmap in bytes
  uint32_t total_frames; // Total number of frames
  uint32_t used_frames;  // Number of frames currently in use
  uint32_t memory_size;  // Total physical memory in bytes

  // Helper functions
  void set_frame(uint32_t frame_addr);
  void clear_frame(uint32_t frame_addr);
  bool test_frame(uint32_t frame_addr);
  int32_t first_free_frame();

public:
  PageFrameAllocator();

  // Initialize PMM with Multiboot memory map
  void initialize(MultibootInfo *mboot_info);

  // Allocate a single frame, returns physical address or 0 on failure
  void *alloc_frame();

  // Allocate a zeroed frame (security: prevents info leakage)
  void *alloc_frame_zeroed();

  // Free a frame (security: double-free detection)
  void free_frame(void *frame_addr);

  // Get memory statistics
  uint32_t get_total_memory() { return memory_size; }
  uint32_t get_total_frames() { return total_frames; }
  uint32_t get_used_frames() { return used_frames; }
  uint32_t get_free_frames() { return total_frames - used_frames; }
};

// Global PMM instance
extern PageFrameAllocator pmm;

#endif // PMM_H
