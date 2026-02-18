#include "sync.h"
#include "priority_scheduler.h"

// ============================================================================
// MOVRAX Synchronization Primitives Implementation
// All wait queues are priority-ordered (highest priority = lowest number first)
// ============================================================================

// ============================================================================
// Spinlock — Interrupt-safe, busy-wait lock
// ============================================================================

void Spinlock::acquire() {
  // Save and disable interrupts
  asm volatile("pushf; pop %0" : "=r"(saved_flags));
  asm volatile("cli");

  // Atomic test-and-set spin
  while (__sync_lock_test_and_set(&lock, 1)) {
    // Spin — in a uniprocessor kernel this should never happen
    // if interrupts are disabled, since no other code can run.
    // This is here for correctness / future SMP support.
    asm volatile("pause"); // Reduce power, hint to CPU
  }
}

void Spinlock::release() {
  __sync_lock_release(&lock);

  // Restore interrupt state
  asm volatile("push %0; popf" : : "r"(saved_flags));
}

// ============================================================================
// Mutex — Priority inheritance protocol
// ============================================================================

void Mutex::init() {
  locked = false;
  owner = nullptr;
  wait_queue_head = nullptr;
  wait_queue_tail = nullptr;
  next_owned = nullptr;
}

void Mutex::add_waiter(Process *proc) {
  // Insert sorted by effective_priority (lowest number = highest priority =
  // front)
  proc->next = nullptr;
  proc->prev = nullptr;

  if (!wait_queue_head) {
    wait_queue_head = proc;
    wait_queue_tail = proc;
    return;
  }

  // Find insertion point
  Process *curr = wait_queue_head;
  while (curr && curr->effective_priority <= proc->effective_priority) {
    curr = curr->next;
  }

  if (!curr) {
    // Insert at tail
    proc->prev = wait_queue_tail;
    wait_queue_tail->next = proc;
    wait_queue_tail = proc;
  } else if (curr == wait_queue_head) {
    // Insert at head
    proc->next = wait_queue_head;
    wait_queue_head->prev = proc;
    wait_queue_head = proc;
  } else {
    // Insert before curr
    proc->prev = curr->prev;
    proc->next = curr;
    curr->prev->next = proc;
    curr->prev = proc;
  }
}

Process *Mutex::remove_highest_waiter() {
  if (!wait_queue_head)
    return nullptr;

  Process *highest = wait_queue_head;
  wait_queue_head = highest->next;

  if (wait_queue_head) {
    wait_queue_head->prev = nullptr;
  } else {
    wait_queue_tail = nullptr;
  }

  highest->next = nullptr;
  highest->prev = nullptr;
  return highest;
}

void Mutex::lock_mutex() {
  CRITICAL_ENTER();

  Process *current = PriorityScheduler::get_current_process();

  if (!locked) {
    // Fast path: uncontested lock
    locked = true;
    owner = current;

    // Add to owner's owned_mutexes list
    next_owned = current->owned_mutexes;
    current->owned_mutexes = this;

    CRITICAL_EXIT();
    return;
  }

  // Slow path: contended — must block
  // Priority inheritance: boost owner if we have higher priority
  if (current->effective_priority < owner->effective_priority) {
    PriorityScheduler::boost_priority(owner, current->effective_priority);
  }

  // Add ourselves to wait queue
  current->blocked_on = this;
  add_waiter(current);

  // Block current process
  PriorityScheduler::block(current);

  CRITICAL_EXIT();

  // Yield CPU — we'll resume after mutex is unlocked
  PriorityScheduler::yield();

  // When we reach here, we own the mutex
}

void Mutex::unlock() {
  CRITICAL_ENTER();

  Process *current = PriorityScheduler::get_current_process();

  if (!locked || owner != current) {
    CRITICAL_EXIT();
    return; // Error: not owner
  }

  // Remove from owner's owned_mutexes list
  Mutex **pp = &current->owned_mutexes;
  while (*pp && *pp != this) {
    pp = &(*pp)->next_owned;
  }
  if (*pp)
    *pp = next_owned;
  next_owned = nullptr;

  // Restore owner's priority (may drop if no other inherited mutexes)
  PriorityScheduler::restore_priority(current);

  // Wake highest-priority waiter
  Process *waiter = remove_highest_waiter();
  if (waiter) {
    // Transfer ownership to waiter
    owner = waiter;
    waiter->blocked_on = nullptr;

    // Add to waiter's owned_mutexes
    next_owned = waiter->owned_mutexes;
    waiter->owned_mutexes = this;

    // Unblock waiter (may trigger preemption)
    PriorityScheduler::unblock(waiter);
  } else {
    // No waiters — just unlock
    locked = false;
    owner = nullptr;
  }

  CRITICAL_EXIT();
}

bool Mutex::try_lock() {
  CRITICAL_ENTER();

  if (!locked) {
    Process *current = PriorityScheduler::get_current_process();
    locked = true;
    owner = current;
    next_owned = current->owned_mutexes;
    current->owned_mutexes = this;

    CRITICAL_EXIT();
    return true;
  }

  CRITICAL_EXIT();
  return false;
}

// ============================================================================
// Semaphore — Counting semaphore with priority-ordered wait queue
// ============================================================================

void Semaphore::init(int32_t initial_count) {
  count = initial_count;
  wait_queue_head = nullptr;
  wait_queue_tail = nullptr;
}

static void sem_add_waiter(Process *&head, Process *&tail, Process *proc) {
  proc->next = nullptr;
  proc->prev = nullptr;

  if (!head) {
    head = proc;
    tail = proc;
    return;
  }

  // Insert sorted by priority
  Process *curr = head;
  while (curr && curr->effective_priority <= proc->effective_priority) {
    curr = curr->next;
  }

  if (!curr) {
    proc->prev = tail;
    tail->next = proc;
    tail = proc;
  } else if (curr == head) {
    proc->next = head;
    head->prev = proc;
    head = proc;
  } else {
    proc->prev = curr->prev;
    proc->next = curr;
    curr->prev->next = proc;
    curr->prev = proc;
  }
}

void Semaphore::wait() {
  CRITICAL_ENTER();

  count--;
  if (count >= 0) {
    // Resource available
    CRITICAL_EXIT();
    return;
  }

  // Must block
  Process *current = PriorityScheduler::get_current_process();
  sem_add_waiter(wait_queue_head, wait_queue_tail, current);
  PriorityScheduler::block(current);

  CRITICAL_EXIT();
  PriorityScheduler::yield();
}

void Semaphore::signal() {
  CRITICAL_ENTER();

  count++;

  if (wait_queue_head) {
    // Wake highest-priority waiter
    Process *waiter = wait_queue_head;
    wait_queue_head = waiter->next;
    if (wait_queue_head)
      wait_queue_head->prev = nullptr;
    else
      wait_queue_tail = nullptr;

    waiter->next = nullptr;
    waiter->prev = nullptr;

    PriorityScheduler::unblock(waiter);
  }

  CRITICAL_EXIT();
}

bool Semaphore::try_wait() {
  CRITICAL_ENTER();

  if (count > 0) {
    count--;
    CRITICAL_EXIT();
    return true;
  }

  CRITICAL_EXIT();
  return false;
}

// ============================================================================
// Condition Variable
// ============================================================================

void CondVar::init() {
  wait_queue_head = nullptr;
  wait_queue_tail = nullptr;
}

void CondVar::wait(Mutex *mutex) {
  CRITICAL_ENTER();

  Process *current = PriorityScheduler::get_current_process();

  // Add to wait queue
  sem_add_waiter(wait_queue_head, wait_queue_tail, current);

  // Release the mutex (allows other threads to make progress)
  // We do this manually to stay in CRITICAL section
  // (can't call mutex->unlock() which would also do CRITICAL_ENTER)
  PriorityScheduler::block(current);

  CRITICAL_EXIT();

  // Release mutex (outside critical section to allow preemption)
  mutex->unlock();

  // Yield — when we wake up, re-acquire mutex
  PriorityScheduler::yield();

  // Re-acquire mutex before returning
  mutex->lock_mutex();
}

void CondVar::signal() {
  CRITICAL_ENTER();

  if (wait_queue_head) {
    Process *waiter = wait_queue_head;
    wait_queue_head = waiter->next;
    if (wait_queue_head)
      wait_queue_head->prev = nullptr;
    else
      wait_queue_tail = nullptr;

    waiter->next = nullptr;
    waiter->prev = nullptr;

    PriorityScheduler::unblock(waiter);
  }

  CRITICAL_EXIT();
}

void CondVar::broadcast() {
  CRITICAL_ENTER();

  while (wait_queue_head) {
    Process *waiter = wait_queue_head;
    wait_queue_head = waiter->next;
    if (wait_queue_head)
      wait_queue_head->prev = nullptr;

    waiter->next = nullptr;
    waiter->prev = nullptr;

    PriorityScheduler::unblock(waiter);
  }
  wait_queue_tail = nullptr;

  CRITICAL_EXIT();
}
