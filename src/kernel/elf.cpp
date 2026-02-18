#include "elf.h"
#include "fs.h"
#include "paging.h"
#include "pmm.h"
#include "priority_scheduler.h"
#include "vga.h"


// ELF Types
#define PT_LOAD 1

// Helper to check magic
static bool check_elf_magic(Elf32_Ehdr *hdr) {
  return hdr->e_ident[0] == 0x7F && hdr->e_ident[1] == 'E' &&
         hdr->e_ident[2] == 'L' && hdr->e_ident[3] == 'F';
}

int ELF::load(const char *path) {
  // 1. Open File
  uint32_t size = fs.get_size(path);
  if (size == 0)
    return -1;

  // Read file header first
  char *buffer = (char *)pmm.alloc_frame();
  uint32_t temp_buffer_vaddr = 0xFFFFD000;
  paging.map_page(temp_buffer_vaddr, (uint32_t)buffer,
                  PAGE_PRESENT | PAGE_WRITABLE);

  char *kernel_buf = (char *)temp_buffer_vaddr;
  fs.read(path, kernel_buf, 4096);

  Elf32_Ehdr *hdr = (Elf32_Ehdr *)kernel_buf;
  if (!check_elf_magic(hdr)) {
    paging.unmap_page(temp_buffer_vaddr);
    pmm.free_frame(buffer);
    return -2; // Invalid ELF
  }

  // 2. Create Address Space
  uint32_t page_dir = paging.create_address_space();
  if (!page_dir) {
    paging.unmap_page(temp_buffer_vaddr);
    pmm.free_frame(buffer);
    return -3; // OOM
  }

  // 3. Load Segments
  Elf32_Phdr *phdr = (Elf32_Phdr *)(kernel_buf + hdr->e_phoff);
  for (int i = 0; i < hdr->e_phnum; i++) {
    if (phdr[i].p_type == PT_LOAD) {
      uint32_t pages = (phdr[i].p_memsz + PAGE_SIZE - 1) / PAGE_SIZE;
      uint32_t vaddr = phdr[i].p_vaddr;

      for (uint32_t j = 0; j < pages; j++) {
        void *phys = pmm.alloc_frame();
        uint32_t paddr = (uint32_t)phys;

        // Map in User Space
        paging.map_page_in_dir(page_dir, vaddr, paddr,
                               PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER);

        // Copy Data
        uint32_t file_offset = phdr[i].p_offset + (j * PAGE_SIZE);
        uint32_t copy_size = 0;
        if (file_offset < phdr[i].p_offset + phdr[i].p_filesz) {
          copy_size = PAGE_SIZE;
          if (file_offset + copy_size > phdr[i].p_offset + phdr[i].p_filesz) {
            copy_size = (phdr[i].p_offset + phdr[i].p_filesz) - file_offset;
          }
        }

        // Map physical frame temporarily to write data
        uint32_t temp_write = 0xFFFFC000;
        paging.map_page(temp_write, paddr, PAGE_PRESENT | PAGE_WRITABLE);

        // Zero page
        for (int k = 0; k < 4096; k++)
          ((char *)temp_write)[k] = 0;

        // Read from file into page
        if (file_offset < 4096 && copy_size > 0) {
          for (uint32_t k = 0; k < copy_size; k++) {
            ((char *)temp_write)[k] = kernel_buf[file_offset + k];
          }
        }

        paging.unmap_page(temp_write);
        vaddr += PAGE_SIZE;
      }
    }
  }

  // 4. Create Stack
  uint32_t stack_vaddr = 0xB0000000;
  void *stack_phys = pmm.alloc_frame();
  paging.map_page_in_dir(page_dir, stack_vaddr - PAGE_SIZE,
                         (uint32_t)stack_phys,
                         PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER);

  // 5. Create User Process using new PriorityScheduler
  int pid = PriorityScheduler::create_process(
      path,                     // Use filename as process name
      (void (*)())hdr->e_entry, // Entry point from ELF header
      PRIORITY_NORMAL,          // Normal priority for user programs
      SCHED_RR,                 // Round-robin time slicing
      false,                    // User mode (Ring 3)
      page_dir                  // Process-specific page directory
  );

  // Cleanup temp buffer
  paging.unmap_page(temp_buffer_vaddr);
  pmm.free_frame(buffer);

  return pid;
}
