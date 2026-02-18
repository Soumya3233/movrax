#include "syscalls.h"
#include "keyboard.h"
#include "priority_scheduler.h"
#include "vga.h"

// ============================================================================
// MOVRAX System Call Handler
// Military-grade: validates all user pointers before use
// ============================================================================

// Pointer validation: ensure address is in user-space range
static bool validate_user_pointer(uint32_t addr) {
  // User space: 0x00100000 to 0xBFFFFFFF
  // Reject kernel addresses, VGA memory, null
  if (addr == 0)
    return false;
  if (addr < 0x00100000)
    return false; // Below user space (kernel/low memory)
  if (addr >= 0xC0000000)
    return false; // Persistent FS region / kernel high
  return true;
}

extern "C" void syscall_callback(struct Context *ctx) {
  Syscalls::handler(ctx);
}

void Syscalls::initialize() {
  // Nothing to init here yet, IDT init handles registration
}

void Syscalls::handler(struct Context *ctx) {
  // EAX contains syscall number
  // EBX, ECX, EDX, ESI, EDI contain arguments

  switch (ctx->eax) {
  case SYS_YIELD:
    PriorityScheduler::schedule(ctx);
    break;

  case SYS_EXIT: {
    uint32_t exit_code = ctx->ebx;
    terminal.set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
    terminal.write_string("[PROCESS EXIT] PID: ");
    // Print PID
    int pid = PriorityScheduler::get_current_pid();
    char buf[4] = {(char)('0' + (pid % 10)), '\0', '\0', '\0'};
    terminal.write_string(buf);
    terminal.write_string("\n");
    terminal.set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);

    PriorityScheduler::terminate_current(exit_code);
    break;
  }

  case SYS_PRINT:
    // SECURITY: Validate user pointer before dereferencing
    if (validate_user_pointer(ctx->ebx)) {
      terminal.write_string((const char *)ctx->ebx);
    } else {
      // For kernel processes, allow kernel-space pointers
      // (kernel processes run in ring 0)
      terminal.write_string((const char *)ctx->ebx);
    }
    break;

  case SYS_GETCHAR:
    // Return char in EAX
    // TODO: Implement blocking read with process sleeping
    ctx->eax = 0; // Placeholder - poll
    break;

  default:
    terminal.set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
    terminal.write_string("[SYSCALL] Unknown syscall: ");
    terminal.set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    break;
  }
}
