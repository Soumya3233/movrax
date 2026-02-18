#ifndef AUDIT_H
#define AUDIT_H

#include "types.h"

// ============================================================================
// MOVRAX Audit Logging Subsystem
// Immutable kernel log stored in persistent memory
// Records all security-relevant events for forensic analysis
// ============================================================================

#define AUDIT_LOG_MAX_ENTRIES 512
#define AUDIT_LOG_MAGIC 0xA0D17106 // "AUDITLOG"

enum AuditEvent : uint8_t {
  AUDIT_BOOT = 0x01,
  AUDIT_PROCESS_CREATE = 0x02,
  AUDIT_PROCESS_TERMINATE = 0x03,
  AUDIT_SYSCALL = 0x04,
  AUDIT_PERMISSION_DENIED = 0x05,
  AUDIT_MEMORY_VIOLATION = 0x06,
  AUDIT_STACK_OVERFLOW = 0x07,
  AUDIT_DEADLINE_MISS = 0x08,
  AUDIT_DOUBLE_FREE = 0x09,
  AUDIT_HEAP_CORRUPTION = 0x0A,
  AUDIT_SECURITY_ALERT = 0x0B,
  AUDIT_INVALID_POINTER = 0x0C,
  AUDIT_WATCHDOG_TIMEOUT = 0x0D
};

// Compact log entry (16 bytes each for alignment)
struct AuditEntry {
  uint32_t timestamp; // Tick count when event occurred
  uint8_t event;      // AuditEvent type
  uint8_t pid;        // Process that triggered the event
  uint16_t data16;    // Event-specific 16-bit data
  uint32_t data32;    // Event-specific 32-bit data (address, syscall#, etc.)
  uint32_t extra;     // Additional context
};

// Audit log header stored in persistent memory
struct AuditLogHeader {
  uint32_t magic;
  uint32_t entry_count; // Total entries written (wraps around)
  uint32_t write_index; // Current write position (ring buffer)
  uint32_t reserved;
};

class AuditLog {
private:
  static AuditLogHeader *header;
  static AuditEntry *entries;
  static bool initialized;

public:
  // Initialize audit log in persistent memory region
  static void initialize(void *persistent_base);

  // Log an event
  static void log(AuditEvent event, uint8_t pid, uint16_t data16,
                  uint32_t data32, uint32_t extra = 0);

  // Convenience functions
  static void log_boot();
  static void log_process_create(uint8_t pid, uint32_t entry_point);
  static void log_process_terminate(uint8_t pid, uint32_t exit_code);
  static void log_security_violation(uint8_t pid, uint32_t fault_address);
  static void log_deadline_miss(uint8_t pid, uint32_t deadline);

  // Dump log to terminal (for forensics)
  static void dump(uint32_t count = 20);

  // Get entry count
  static uint32_t get_count();
};

#endif // AUDIT_H
