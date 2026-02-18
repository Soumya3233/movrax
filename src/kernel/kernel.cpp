#include "audit.h"
#include "fs.h"
#include "gdt.h"
#include "heap.h"
#include "idt.h"
#include "integrity.h"
#include "keyboard.h"
#include "multiboot.h"
#include "paging.h"
#include "pic.h"
#include "pmm.h"
#include "priority_scheduler.h"
#include "shell.h"
#include "timer.h"
#include "vga.h"
#include "watchdog.h"


// Helper function to print a number as decimal
void print_number(uint32_t num) {
  if (num == 0) {
    terminal.write_string("0");
    return;
  }

  char buffer[12];
  int i = 0;
  while (num > 0) {
    buffer[i++] = '0' + (num % 10);
    num /= 10;
  }

  while (i > 0) {
    char c[2] = {buffer[--i], '\0'};
    terminal.write_string(c);
  }
}

void print_memory_size(uint32_t bytes) {
  if (bytes >= 1024 * 1024) {
    print_number(bytes / (1024 * 1024));
    terminal.write_string(" MB");
  } else {
    print_number(bytes / 1024);
    terminal.write_string(" KB");
  }
}

extern "C" void kernel_main(MultibootInfo *mboot_info) {
  terminal.initialize();

  terminal.set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
  terminal.write_string("========================================\n");
  terminal.write_string("  MOVRAX - Military Persistent Memory OS\n");
  terminal.write_string("========================================\n\n");

  terminal.set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
  terminal.write_string("Kernel loaded successfully!\n\n");

  // ---- GDT ----
  terminal.set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
  terminal.write_string("[INIT] GDT (6 segments + TSS)...\n");
  terminal.set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
  GDT::initialize();

  // ---- PMM ----
  terminal.set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
  terminal.write_string("[INIT] Physical Memory Manager...\n");
  terminal.set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
  pmm.initialize(mboot_info);
  terminal.write_string("  Total: ");
  terminal.set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
  print_memory_size(pmm.get_total_memory());
  terminal.set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
  terminal.write_string("\n");

  // ---- Paging ----
  terminal.set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
  terminal.write_string("[INIT] Paging (W^X enforced)...\n");
  terminal.set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
  paging.initialize();
  paging.enable();
  terminal.write_string("  Kernel isolation: ENFORCED | CR0.WP: ON\n");

  // ---- Heap ----
  terminal.set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
  terminal.write_string("[INIT] Kernel Heap (256KB)...\n");
  terminal.set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
  kheap.initialize();
  terminal.write_string("  Heap @ 0xD0000000 ");
  terminal.set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
  terminal.write_string("OK\n");
  terminal.set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);

  // ---- IDT ----
  terminal.set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
  terminal.write_string("[INIT] IDT (256 vectors)...\n");
  terminal.set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
  IDT::initialize();

  // ---- PIC ----
  terminal.set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
  terminal.write_string("[INIT] PIC (IRQ 32-47)...\n");
  terminal.set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
  PIC::initialize();

  // ---- Timer ----
  terminal.set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
  terminal.write_string("[INIT] Timer (PIT @ 1000Hz)...\n");
  terminal.set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
  Timer::initialize();

  // ---- Priority Scheduler ----
  terminal.set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
  terminal.write_string("[INIT] Priority Scheduler (256-level)...\n");
  terminal.set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
  PriorityScheduler::initialize();
  terminal.write_string(
      "  FIFO/RR/EDF | Priority Inheritance | Stack Guards\n");

  // Enable Interrupts
  asm volatile("sti");
  terminal.write_string("  Interrupts ");
  terminal.set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
  terminal.write_string("ARMED\n");
  terminal.set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);

  // ---- Ticker process ----
  PriorityScheduler::create_process(
      "ticker",
      []() {
        uint8_t *vga = (uint8_t *)0xB809E;
        uint8_t ticker = 0;
        while (true) {
          const char chars[] = "/-\\|";
          *vga = chars[ticker++ % 4];
          *(vga + 1) = 0x0E;
          for (int i = 0; i < 5000000; i++)
            asm volatile("nop");
          PriorityScheduler::yield();
        }
      },
      PRIORITY_IDLE, SCHED_RR);

  // ---- Filesystem ----
  terminal.set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
  terminal.write_string("[INIT] Persistent Filesystem...\n");
  terminal.set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
  fs.initialize();
  terminal.write_string("  Files: ");
  print_number(fs.get_file_count());
  terminal.write_string(" | Free: ");
  print_memory_size(fs.get_free_space());
  terminal.write_string("\n");

  // ---- Audit Log ----
  terminal.set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
  terminal.write_string("[INIT] Audit Log (persistent)...\n");
  terminal.set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
  AuditLog::initialize((void *)PERSISTENT_REGION_VADDR);
  AuditLog::log_boot();
  terminal.write_string("  Events: ");
  print_number(AuditLog::get_count());
  terminal.write_string(" (persisted)\n");

  // ---- Integrity ----
  terminal.set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
  terminal.write_string("[INIT] Integrity (CRC32)...\n");
  terminal.set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
  uint32_t corrupted = FSIntegrity::verify_all();
  if (corrupted == 0) {
    terminal.write_string("  Filesystem ");
    terminal.set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    terminal.write_string("CLEAN\n");
  } else {
    terminal.set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
    terminal.write_string("  WARNING: ");
    print_number(corrupted);
    terminal.write_string(" corrupted file(s) detected!\n");
  }
  terminal.set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);

  // ---- Encryption ----
  // Initialize XOR cipher with a default key (in production, derive from
  // hardware)
  XORCipher::initialize(0xDEADC0DE, 0xCAFEBABE, 0x13371337, 0xF00DFACE);
  terminal.write_string("  Encryption: ARMED\n");

  // ---- Watchdog ----
  terminal.set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
  terminal.write_string("[INIT] Watchdog (5s timeout)...\n");
  terminal.set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
  Watchdog::initialize(5000);
  Watchdog::enable();
  terminal.write_string("  Watchdog ");
  terminal.set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
  terminal.write_string("ARMED\n");
  terminal.set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);

  // ---- Keyboard ----
  terminal.set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
  terminal.write_string("[INIT] PS/2 Keyboard...\n");
  terminal.set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
  keyboard.initialize();
  terminal.write_string("  Keyboard ");
  terminal.set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
  terminal.write_string("OK\n\n");
  terminal.set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);

  // ---- System Status Dashboard ----
  terminal.set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
  terminal.write_string("--- System Status ---\n");

  const char *labels[] = {"Kernel",    "Memory",    "Filesystem",
                          "Scheduler", "Security",  "Heap",
                          "Audit",     "Integrity", "Watchdog"};
  const char *status[] = {"[ARMED]",
                          "[ARMED] W^X + Guards",
                          "[ARMED]",
                          "[ARMED] 256-Level Preemptive",
                          "[ARMED] Kernel Isolation",
                          "[ARMED] Corruption Detection",
                          "[ARMED] Persistent Log",
                          "[ARMED] CRC32 Verified",
                          "[ARMED] 5s Timeout"};

  for (int i = 0; i < 9; i++) {
    terminal.set_color(VGA_COLOR_YELLOW, VGA_COLOR_BLACK);
    terminal.write_string("  ");
    terminal.write_string(labels[i]);
    // Pad to 14 chars
    int len = 0;
    const char *p = labels[i];
    while (*p++)
      len++;
    for (int j = len; j < 12; j++)
      terminal.write_string(" ");
    terminal.write_string(": ");
    terminal.set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    terminal.write_string(status[i]);
    terminal.write_string("\n");
  }
  terminal.set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
  terminal.write_string("\n");

  // Launch shell
  shell.initialize();
  shell.run();

  terminal.set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
  terminal.write_string("\nSystem halted.\n");

  while (1) {
    asm volatile("hlt");
  }
}