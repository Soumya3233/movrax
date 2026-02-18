#ifndef WATCHDOG_H
#define WATCHDOG_H

#include "types.h"

// ============================================================================
// MOVRAX Hardware Watchdog Timer
// Monitors kernel health — resets system if kernel hangs
// Military requirement: system must never silently freeze
// ============================================================================

#define WATCHDOG_DEFAULT_TIMEOUT 5000 // 5 seconds default
#define WATCHDOG_MIN_TIMEOUT 500      // 500ms minimum
#define WATCHDOG_MAGIC 0x57444F47     // "WDOG"

class Watchdog {
private:
  static uint32_t timeout_ticks; // Timeout in timer ticks
  static uint32_t last_kick;     // Last time watchdog was kicked
  static bool enabled;
  static uint32_t kick_count;      // Total kicks (diagnostic)
  static uint32_t violation_count; // Times watchdog triggered

public:
  // Initialize watchdog with timeout in milliseconds
  static void initialize(uint32_t timeout_ms = WATCHDOG_DEFAULT_TIMEOUT);

  // Kick (reset) the watchdog timer — must be called periodically
  static void kick();

  // Check if watchdog has expired (called from timer interrupt)
  static void check();

  // Enable/disable
  static void enable();
  static void disable();

  // Get status
  static bool is_enabled() { return enabled; }
  static uint32_t get_remaining(); // Ticks until expiry
  static uint32_t get_kick_count() { return kick_count; }
  static uint32_t get_violations() { return violation_count; }
};

#endif // WATCHDOG_H
