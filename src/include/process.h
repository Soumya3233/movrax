#ifndef PROCESS_H
#define PROCESS_H

// ============================================================================
// BACKWARD COMPATIBILITY SHIM
// This header redirects to the new priority_scheduler.h
// Old code using Scheduler:: will use the PriorityScheduler via typedef
// ============================================================================

#include "priority_scheduler.h"

// Legacy compatibility: map old Scheduler API to PriorityScheduler
class Scheduler {
public:
  static void initialize() { PriorityScheduler::initialize(); }

  static int create_kernel_process(void (*entry_point)()) {
    return PriorityScheduler::create_process("kernel_task", entry_point,
                                             PRIORITY_NORMAL, SCHED_RR);
  }

  static int create_user_process(void (*entry_point)(), uint32_t cr3 = 0) {
    return PriorityScheduler::create_process(
        "user_task", entry_point, PRIORITY_NORMAL, SCHED_RR, false, cr3);
  }

  static void schedule(struct Context *ctx) {
    PriorityScheduler::schedule(ctx);
  }

  static void yield() { PriorityScheduler::yield(); }

  static int get_current_pid() { return PriorityScheduler::get_current_pid(); }
};

#endif // PROCESS_H
