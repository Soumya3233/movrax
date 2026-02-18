#include "audit.h"
#include "timer.h"
#include "vga.h"

// ============================================================================
// MOVRAX Audit Log Implementation
// Ring buffer in persistent memory — survives reboot
// ============================================================================

AuditLogHeader *AuditLog::header = nullptr;
AuditEntry *AuditLog::entries = nullptr;
bool AuditLog::initialized = false;

// Helper to print hex
static void print_hex_audit(uint32_t val) {
  const char hex[] = "0123456789ABCDEF";
  char buf[11] = "0x00000000";
  for (int i = 9; i >= 2; i--) {
    buf[i] = hex[val & 0xF];
    val >>= 4;
  }
  terminal.write_string(buf);
}

static void print_num_audit(uint32_t num) {
  if (num == 0) {
    terminal.write_string("0");
    return;
  }
  char buf[12];
  int i = 0;
  while (num > 0) {
    buf[i++] = '0' + (num % 10);
    num /= 10;
  }
  while (i > 0) {
    char c[2] = {buf[--i], '\0'};
    terminal.write_string(c);
  }
}

void AuditLog::initialize(void *persistent_base) {
  // Audit log lives at end of persistent FS region
  // Use last 16KB of persistent memory (512 entries * 16 bytes + header)
  uint8_t *base = (uint8_t *)persistent_base + (1024 * 1024) - (16 * 1024);

  header = (AuditLogHeader *)base;
  entries = (AuditEntry *)(base + sizeof(AuditLogHeader));

  // Check if already initialized (persistent across reboots)
  if (header->magic != AUDIT_LOG_MAGIC) {
    // First boot: initialize
    header->magic = AUDIT_LOG_MAGIC;
    header->entry_count = 0;
    header->write_index = 0;
    header->reserved = 0;
  }

  initialized = true;
}

void AuditLog::log(AuditEvent event, uint8_t pid, uint16_t data16,
                   uint32_t data32, uint32_t extra) {
  if (!initialized)
    return;

  AuditEntry *entry = &entries[header->write_index];
  entry->timestamp = Timer::get_ticks();
  entry->event = (uint8_t)event;
  entry->pid = pid;
  entry->data16 = data16;
  entry->data32 = data32;
  entry->extra = extra;

  header->write_index = (header->write_index + 1) % AUDIT_LOG_MAX_ENTRIES;
  header->entry_count++;
}

void AuditLog::log_boot() { log(AUDIT_BOOT, 0, 0, Timer::get_ticks()); }

void AuditLog::log_process_create(uint8_t pid, uint32_t entry_point) {
  log(AUDIT_PROCESS_CREATE, pid, 0, entry_point);
}

void AuditLog::log_process_terminate(uint8_t pid, uint32_t exit_code) {
  log(AUDIT_PROCESS_TERMINATE, pid, 0, exit_code);
}

void AuditLog::log_security_violation(uint8_t pid, uint32_t fault_address) {
  log(AUDIT_MEMORY_VIOLATION, pid, 0, fault_address);
}

void AuditLog::log_deadline_miss(uint8_t pid, uint32_t deadline) {
  log(AUDIT_DEADLINE_MISS, pid, 0, deadline);
}

uint32_t AuditLog::get_count() {
  if (!initialized)
    return 0;
  return header->entry_count;
}

// Event name lookup
static const char *event_name(uint8_t event) {
  switch (event) {
  case AUDIT_BOOT:
    return "BOOT";
  case AUDIT_PROCESS_CREATE:
    return "PROC_CREATE";
  case AUDIT_PROCESS_TERMINATE:
    return "PROC_TERM";
  case AUDIT_SYSCALL:
    return "SYSCALL";
  case AUDIT_PERMISSION_DENIED:
    return "PERM_DENY";
  case AUDIT_MEMORY_VIOLATION:
    return "MEM_VIOL";
  case AUDIT_STACK_OVERFLOW:
    return "STK_OFLOW";
  case AUDIT_DEADLINE_MISS:
    return "DL_MISS";
  case AUDIT_DOUBLE_FREE:
    return "DBL_FREE";
  case AUDIT_HEAP_CORRUPTION:
    return "HEAP_CORR";
  case AUDIT_SECURITY_ALERT:
    return "SEC_ALERT";
  case AUDIT_INVALID_POINTER:
    return "INV_PTR";
  case AUDIT_WATCHDOG_TIMEOUT:
    return "WDG_TOUT";
  default:
    return "UNKNOWN";
  }
}

void AuditLog::dump(uint32_t count) {
  if (!initialized) {
    terminal.write_string("Audit log not initialized\n");
    return;
  }

  terminal.set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
  terminal.write_string("=== AUDIT LOG (");
  print_num_audit(header->entry_count);
  terminal.write_string(" total events) ===\n");

  terminal.set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
  terminal.write_string("  TICK      EVENT        PID  DATA\n");
  terminal.set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
  terminal.write_string("  ----      -----        ---  ----\n");

  // Show last N entries
  uint32_t total = header->entry_count;
  if (count > total)
    count = total;
  if (count > AUDIT_LOG_MAX_ENTRIES)
    count = AUDIT_LOG_MAX_ENTRIES;

  uint32_t start_idx;
  if (total <= AUDIT_LOG_MAX_ENTRIES) {
    start_idx = (total >= count) ? total - count : 0;
  } else {
    start_idx = (header->write_index + AUDIT_LOG_MAX_ENTRIES - count) %
                AUDIT_LOG_MAX_ENTRIES;
  }

  for (uint32_t i = 0; i < count; i++) {
    uint32_t idx = (start_idx + i) % AUDIT_LOG_MAX_ENTRIES;
    AuditEntry *e = &entries[idx];

    // Color code by severity
    if (e->event >= AUDIT_PERMISSION_DENIED &&
        e->event <= AUDIT_WATCHDOG_TIMEOUT) {
      terminal.set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
    } else {
      terminal.set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    }

    terminal.write_string("  ");
    print_num_audit(e->timestamp);

    // Pad to 10 chars
    terminal.write_string("  ");
    terminal.write_string(event_name(e->event));

    terminal.write_string("  P");
    print_num_audit(e->pid);

    terminal.write_string("  ");
    print_hex_audit(e->data32);
    terminal.write_string("\n");
  }

  terminal.set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
}
