#include "watchdog.h"
#include "audit.h"
#include "timer.h"
#include "vga.h"


// ============================================================================
// MOVRAX Watchdog Timer Implementation
// Software watchdog — checked every timer tick (1ms at 1000Hz)
// If not kicked within timeout, logs violation and triggers recovery
// ============================================================================

uint32_t Watchdog::timeout_ticks = WATCHDOG_DEFAULT_TIMEOUT;
uint32_t Watchdog::last_kick = 0;
bool Watchdog::enabled = false;
uint32_t Watchdog::kick_count = 0;
uint32_t Watchdog::violation_count = 0;

void Watchdog::initialize(uint32_t timeout_ms) {
  if (timeout_ms < WATCHDOG_MIN_TIMEOUT) {
    timeout_ms = WATCHDOG_MIN_TIMEOUT;
  }
  timeout_ticks = timeout_ms; // 1 tick = 1ms at 1000Hz PIT
  last_kick = Timer::get_ticks();
  kick_count = 0;
  violation_count = 0;
  enabled = false; // Must be explicitly enabled
}

void Watchdog::kick() {
  last_kick = Timer::get_ticks();
  kick_count++;
}

void Watchdog::enable() {
  last_kick = Timer::get_ticks();
  enabled = true;
}

void Watchdog::disable() { enabled = false; }

uint32_t Watchdog::get_remaining() {
  if (!enabled)
    return 0xFFFFFFFF;
  uint32_t elapsed = Timer::get_ticks() - last_kick;
  if (elapsed >= timeout_ticks)
    return 0;
  return timeout_ticks - elapsed;
}

void Watchdog::check() {
  if (!enabled)
    return;

  uint32_t current = Timer::get_ticks();
  uint32_t elapsed = current - last_kick;

  if (elapsed >= timeout_ticks) {
    violation_count++;

    // Log to audit
    AuditLog::log(AUDIT_WATCHDOG_TIMEOUT, 0, (uint16_t)violation_count,
                  elapsed);

    // Visual alert
    terminal.set_color(VGA_COLOR_WHITE, VGA_COLOR_RED);
    terminal.write_string("\n[WATCHDOG] TIMEOUT! Kernel unresponsive for ");

    // Print elapsed seconds
    uint32_t secs = elapsed / 1000;
    char buf[12];
    int i = 0;
    if (secs == 0)
      buf[i++] = '0';
    else {
      uint32_t v = secs;
      while (v) {
        buf[i++] = '0' + (v % 10);
        v /= 10;
      }
    }
    while (i > 0) {
      char c[2] = {buf[--i], '\0'};
      terminal.write_string(c);
    }
    terminal.write_string("s\n");

    if (violation_count >= 3) {
      terminal.write_string("[WATCHDOG] 3 violations — SYSTEM HALT\n");
      terminal.set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
      // Hard halt — in production, this could trigger hardware reset
      asm volatile("cli; hlt");
    }

    terminal.set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);

    // Reset the kick timer to give system a chance to recover
    last_kick = current;
  }
}
