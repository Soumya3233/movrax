#ifndef SYNC_H
#define SYNC_H

#include "types.h"

// ============================================================================
// MOVRAX Synchronization Primitives
// Priority inheritance to prevent priority inversion
// ============================================================================

// Forward declaration
struct Process;

// ---------- Spinlock (interrupt-context safe) ----------
struct Spinlock {
  volatile uint32_t lock;
  uint32_t saved_flags; // Saved interrupt state

  void init() {
    lock = 0;
    saved_flags = 0;
  }
  void acquire();
  void release();
};

// ---------- Mutex (process-context, supports priority inheritance) ----------
struct Mutex {
  bool locked;
  Process *owner; // Process currently holding the mutex
  Process
      *wait_queue_head; // Linked list of waiting processes (priority-ordered)
  Process *wait_queue_tail;
  Mutex *next_owned; // Next mutex in owner's owned_mutexes list

  void init();

  // Blocking lock - caller may be boosted via priority inheritance
  void lock_mutex();

  // Unlock - restores owner's priority, wakes highest-priority waiter
  void unlock();

  // Try lock - non-blocking, returns true if acquired
  bool try_lock();

  // Add process to wait queue (sorted by priority, highest first)
  void add_waiter(Process *proc);

  // Remove and return highest-priority waiter
  Process *remove_highest_waiter();
};

// ---------- Semaphore (counting, with priority-ordered wait queue) ----------
struct Semaphore {
  int32_t count;
  Process *wait_queue_head;
  Process *wait_queue_tail;

  void init(int32_t initial_count);

  // Decrement (block if count <= 0)
  void wait();

  // Increment (wake one waiter if any)
  void signal();

  // Try wait - non-blocking, returns true if decremented
  bool try_wait();
};

// ---------- Condition Variable ----------
struct CondVar {
  Process *wait_queue_head;
  Process *wait_queue_tail;

  void init();

  // Wait on condition (must hold mutex, releases it while waiting)
  void wait(Mutex *mutex);

  // Wake one waiter
  void signal();

  // Wake all waiters
  void broadcast();
};

#endif // SYNC_H
