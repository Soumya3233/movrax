#include "priority_scheduler.h"
#include "gdt.h"
#include "sync.h"
#include "vga.h"

// ============================================================================
// MOVRAX Priority-Based Preemptive Scheduler Implementation
// ============================================================================

// ---------- Static member definitions ----------
Process PriorityScheduler::process_table[MAX_PROCESSES];
RunQueue PriorityScheduler::run_queue;
int PriorityScheduler::current_pid = -1;
int PriorityScheduler::process_count = 0;
bool PriorityScheduler::enabled = false;
bool PriorityScheduler::need_reschedule = false;

// ---------- Helper: Memory operations ----------
static void mem_zero(void *dest, uint32_t size) {
  uint8_t *d = (uint8_t *)dest;
  for (uint32_t i = 0; i < size; i++)
    d[i] = 0;
}

static void str_copy_safe(char *dest, const char *src, uint32_t max) {
  uint32_t i = 0;
  while (src[i] && i < max - 1) {
    dest[i] = src[i];
    i++;
  }
  dest[i] = '\0';
}

// ============================================================================
// Run Queue Operations — O(1) Bitmap Scheduler
// ============================================================================

static inline int find_first_set_bit(uint32_t *bitmap, int words) {
  for (int i = 0; i < words; i++) {
    if (bitmap[i] != 0) {
      // Use BSF (Bit Scan Forward) for O(1) on x86
      uint32_t bit;
      asm volatile("bsf %1, %0" : "=r"(bit) : "r"(bitmap[i]));
      return i * 32 + bit;
    }
  }
  return -1; // No ready process
}

void PriorityScheduler::enqueue(Process *proc) {
  uint8_t prio = proc->effective_priority;

  proc->next = nullptr;
  proc->prev = nullptr;

  if (run_queue.tails[prio]) {
    // Append to tail of this priority's list
    run_queue.tails[prio]->next = proc;
    proc->prev = run_queue.tails[prio];
    run_queue.tails[prio] = proc;
  } else {
    // First process at this priority
    run_queue.heads[prio] = proc;
    run_queue.tails[prio] = proc;
  }

  // Set bitmap bit
  uint32_t word = prio / 32;
  uint32_t bit = prio % 32;
  run_queue.bitmap[word] |= (1 << bit);
}

void PriorityScheduler::dequeue(Process *proc) {
  uint8_t prio = proc->effective_priority;

  if (proc->prev)
    proc->prev->next = proc->next;
  if (proc->next)
    proc->next->prev = proc->prev;

  if (run_queue.heads[prio] == proc)
    run_queue.heads[prio] = proc->next;
  if (run_queue.tails[prio] == proc)
    run_queue.tails[prio] = proc->prev;

  proc->next = nullptr;
  proc->prev = nullptr;

  // Clear bitmap bit if no more processes at this priority
  if (!run_queue.heads[prio]) {
    uint32_t word = prio / 32;
    uint32_t bit = prio % 32;
    run_queue.bitmap[word] &= ~(1 << bit);
  }
}

Process *PriorityScheduler::pick_next() {
  // O(1): Find highest-priority (lowest number) with bitmap scan
  int prio = find_first_set_bit(run_queue.bitmap, PRIORITY_LEVELS / 32);
  if (prio < 0)
    return nullptr;

  return run_queue.heads[prio]; // Head of the queue at this priority
}

// ============================================================================
// Initialization
// ============================================================================

void PriorityScheduler::initialize() {
  // Clear all process slots
  for (int i = 0; i < MAX_PROCESSES; i++) {
    process_table[i].state = PROCESS_UNUSED;
    process_table[i].pid = i;
    process_table[i].stack_guard = GUARD_STACK_MAGIC;
  }

  // Clear run queue
  mem_zero(&run_queue, sizeof(RunQueue));

  // Process 0: Current kernel execution (idle/init)
  Process *p0 = &process_table[0];
  p0->pid = 0;
  p0->state = PROCESS_RUNNING;
  p0->base_priority = PRIORITY_IDLE;
  p0->effective_priority = PRIORITY_IDLE;
  p0->policy = SCHED_FIFO;
  p0->time_slice = 0;
  p0->time_slice_max = 0;
  p0->blocked_on = nullptr;
  p0->owned_mutexes = nullptr;
  p0->next = nullptr;
  p0->prev = nullptr;
  str_copy_safe(p0->name, "kernel_init", 32);
  asm volatile("mov %%cr3, %0" : "=r"(p0->cr3));

  current_pid = 0;
  process_count = 1;
  enabled = true;
  need_reschedule = false;
}

// ============================================================================
// Process Allocation & Deallocation
// ============================================================================

Process *PriorityScheduler::alloc_process() {
  for (int i = 0; i < MAX_PROCESSES; i++) {
    if (process_table[i].state == PROCESS_UNUSED) {
      return &process_table[i];
    }
  }
  return nullptr;
}

void PriorityScheduler::free_process(Process *proc) {
  CRITICAL_ENTER();

  if (proc->state == PROCESS_READY) {
    dequeue(proc);
  }

  proc->state = PROCESS_UNUSED;
  proc->blocked_on = nullptr;
  proc->owned_mutexes = nullptr;
  proc->next = nullptr;
  proc->prev = nullptr;
  proc->stack_guard = GUARD_STACK_MAGIC;

  CRITICAL_EXIT();
}

// ============================================================================
// Stack Setup for New Processes
// ============================================================================

// External functions from context_switch.asm
extern "C" void process_entry_trampoline();
extern "C" void enter_usermode(uint32_t entry, uint32_t user_esp);

void PriorityScheduler::setup_kernel_stack(Process *proc,
                                           void (*entry_point)()) {
  // The context switch saves/restores: EDI, ESI, EBX, EBP
  // Then 'ret' pops EIP. For a new process, we set up a fake frame
  // so that switch_context's 'ret' jumps to the trampoline,
  // and the trampoline's 'ret' jumps to entry_point.

  uint32_t *stack_top = (uint32_t *)&proc->stack[STACK_SIZE];

  // Push entry point (trampoline will 'ret' into this)
  *(--stack_top) = (uint32_t)entry_point;

  // Push trampoline address (switch_context's 'ret' jumps here)
  *(--stack_top) = (uint32_t)process_entry_trampoline;

  // Fake callee-saved registers (switch_context will pop these)
  *(--stack_top) = 0; // EBP
  *(--stack_top) = 0; // EBX
  *(--stack_top) = 0; // ESI
  *(--stack_top) = 0; // EDI

  proc->esp = (uint32_t)stack_top;
}

void PriorityScheduler::setup_user_stack(Process *proc, void (*entry_point)(),
                                         uint32_t user_esp) {
  // For Ring 3 processes, we set up the kernel stack so that
  // switch_context → trampoline → enter_usermode(entry_point, user_esp)
  // The enter_usermode function builds an IRET frame for Ring 3 transition

  uint32_t *stack_top = (uint32_t *)&proc->stack[STACK_SIZE];

  // Arguments for enter_usermode (pushed in reverse)
  *(--stack_top) = user_esp;
  *(--stack_top) = (uint32_t)entry_point;
  *(--stack_top) = 0; // Fake return address (enter_usermode never returns)

  // Push enter_usermode address (trampoline will call this)
  *(--stack_top) = (uint32_t)enter_usermode;

  // Push trampoline
  *(--stack_top) = (uint32_t)process_entry_trampoline;

  // Fake callee-saved registers
  *(--stack_top) = 0; // EBP
  *(--stack_top) = 0; // EBX
  *(--stack_top) = 0; // ESI
  *(--stack_top) = 0; // EDI

  proc->esp = (uint32_t)stack_top;
}

// ============================================================================
// Process Creation
// ============================================================================

int PriorityScheduler::create_process(const char *name, void (*entry_point)(),
                                      uint8_t priority, SchedPolicy policy,
                                      bool kernel_mode, uint32_t cr3) {
  CRITICAL_ENTER();

  Process *proc = alloc_process();
  if (!proc) {
    CRITICAL_EXIT();
    return -1;
  }

  int pid = proc->pid;

  // Initialize PCB
  str_copy_safe(proc->name, name, 32);
  proc->state = PROCESS_READY;
  proc->exit_code = 0;
  proc->base_priority = priority;
  proc->effective_priority = priority;
  proc->policy = policy;
  proc->time_slice = (policy == SCHED_RR) ? DEFAULT_TIME_SLICE : 0;
  proc->time_slice_max = proc->time_slice;
  proc->deadline = 0;
  proc->period = 0;
  proc->sleep_until = 0;
  proc->blocked_on = nullptr;
  proc->owned_mutexes = nullptr;
  proc->stack_guard = GUARD_STACK_MAGIC;

  // Set address space
  if (cr3 != 0) {
    proc->cr3 = cr3;
  } else {
    asm volatile("mov %%cr3, %0" : "=r"(proc->cr3));
  }

  // Setup stack
  if (kernel_mode) {
    setup_kernel_stack(proc, entry_point);
  } else {
    // For user mode, we need a user stack address
    // Default to using part of the process kernel stack (for testing)
    uint32_t user_esp = (uint32_t)&proc->stack[STACK_SIZE - 256];
    setup_user_stack(proc, entry_point, user_esp);
  }

  // Add to run queue
  enqueue(proc);
  process_count++;

  // Check if we need to preempt current process
  if (enabled && current_pid >= 0) {
    Process *current = &process_table[current_pid];
    if (priority < current->effective_priority) {
      need_reschedule = true;
    }
  }

  CRITICAL_EXIT();
  return pid;
}

// ============================================================================
// Process Termination
// ============================================================================

void PriorityScheduler::terminate(int pid, uint32_t exit_code) {
  if (pid < 0 || pid >= MAX_PROCESSES)
    return;

  CRITICAL_ENTER();

  Process *proc = &process_table[pid];
  if (proc->state == PROCESS_UNUSED) {
    CRITICAL_EXIT();
    return;
  }

  proc->state = PROCESS_TERMINATED;
  proc->exit_code = exit_code;

  // Release all owned mutexes
  Mutex *m = proc->owned_mutexes;
  while (m) {
    Mutex *next = m->next_owned;
    m->unlock();
    m = next;
  }
  proc->owned_mutexes = nullptr;

  // Remove from run queue
  dequeue(proc);

  // Free the process slot
  free_process(proc);
  process_count--;

  CRITICAL_EXIT();

  // If terminating self, must schedule immediately
  if (pid == current_pid) {
    yield();
  }
}

void PriorityScheduler::terminate_current(uint32_t exit_code) {
  terminate(current_pid, exit_code);
}

// ============================================================================
// Core Scheduler — Called by Timer Interrupt
// ============================================================================

void PriorityScheduler::schedule(struct Context *ctx) {
  (void)ctx; // Parameter reserved for future register-state use
  if (!enabled)
    return;

  Process *current = &process_table[current_pid];

  // Decrement time slice for SCHED_RR processes
  if (current->policy == SCHED_RR && current->state == PROCESS_RUNNING) {
    if (current->time_slice > 0) {
      current->time_slice--;
    }

    // Time slice expired: move to back of same priority queue
    if (current->time_slice == 0) {
      current->time_slice = current->time_slice_max;
      current->state = PROCESS_READY;
      enqueue(current);
    }
  }

  // Check EDF deadlines
  uint32_t now = Timer::get_ticks();
  for (int i = 0; i < MAX_PROCESSES; i++) {
    Process *p = &process_table[i];
    if (p->policy == SCHED_EDF && p->state != PROCESS_UNUSED &&
        p->deadline > 0 && now > p->deadline) {
      missed_deadline_handler(p);
    }
  }

  // Wake sleeping processes
  for (int i = 0; i < MAX_PROCESSES; i++) {
    Process *p = &process_table[i];
    if (p->state == PROCESS_SLEEPING && now >= p->sleep_until) {
      p->state = PROCESS_READY;
      enqueue(p);
    }
  }

  // Pick highest-priority ready process
  Process *next = pick_next();
  if (!next) {
    // No ready process — continue running current (idle)
    return;
  }

  // If current is still running and higher/equal priority, keep it
  if (current->state == PROCESS_RUNNING) {
    if (current->effective_priority <= next->effective_priority) {
      return; // Current is same or higher priority, keep running
    }
    // Current preempted by higher-priority process
    current->state = PROCESS_READY;
    enqueue(current);
  }

  // Dequeue next from run queue
  dequeue(next);
  next->state = PROCESS_RUNNING;

  // Update TSS for ring transitions
  uint32_t kernel_stack_top = (uint32_t)(next->stack + STACK_SIZE);
  GDT::set_tss_stack(kernel_stack_top);

  // Perform context switch
  int old_pid = current_pid;
  current_pid = next->pid;

  switch_context(&process_table[old_pid].esp, next->esp, next->cr3);

  // NOTE: Execution resumes here when this process is switched BACK to
  need_reschedule = false;
}

// ============================================================================
// Voluntary Operations
// ============================================================================

void PriorityScheduler::yield() { asm volatile("int $0x80"); }

void PriorityScheduler::sleep(uint32_t ms) {
  CRITICAL_ENTER();

  Process *current = &process_table[current_pid];
  current->state = PROCESS_SLEEPING;
  current->sleep_until = Timer::get_ticks() + ms;

  CRITICAL_EXIT();

  yield();
}

void PriorityScheduler::block(Process *proc) {
  // Called with interrupts already disabled
  proc->state = PROCESS_BLOCKED;
  // Do NOT enqueue — blocked processes are in the mutex's wait queue
}

void PriorityScheduler::unblock(Process *proc) {
  // Called with interrupts already disabled
  proc->state = PROCESS_READY;
  enqueue(proc);

  // Check if unblocked process preempts current
  if (current_pid >= 0) {
    Process *current = &process_table[current_pid];
    if (proc->effective_priority < current->effective_priority) {
      need_reschedule = true;
    }
  }
}

// ============================================================================
// EDF Deadline Management
// ============================================================================

void PriorityScheduler::set_deadline(int pid, uint32_t deadline_ticks,
                                     uint32_t period_ticks) {
  if (pid < 0 || pid >= MAX_PROCESSES)
    return;

  CRITICAL_ENTER();

  Process *proc = &process_table[pid];
  proc->deadline = Timer::get_ticks() + deadline_ticks;
  proc->period = period_ticks;

  CRITICAL_EXIT();
}

void PriorityScheduler::missed_deadline_handler(Process *proc) {
  // Log the missed deadline (critical in military systems)
  terminal.set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
  terminal.write_string("[DEADLINE MISS] Process: ");
  terminal.write_string(proc->name);
  terminal.write_string("\n");
  terminal.set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);

  // Reset deadline for next period
  if (proc->period > 0) {
    proc->deadline += proc->period;
  }

  // TODO: In production — trigger watchdog alert, possibly restart process
}

// ============================================================================
// Priority Inheritance
// ============================================================================

void PriorityScheduler::boost_priority(Process *proc, uint8_t new_priority) {
  if (new_priority >= proc->effective_priority)
    return; // Only boost upward

  // If process is in run queue, must dequeue and re-enqueue at new priority
  if (proc->state == PROCESS_READY) {
    dequeue(proc);
    proc->effective_priority = new_priority;
    enqueue(proc);
  } else {
    proc->effective_priority = new_priority;
  }
}

void PriorityScheduler::restore_priority(Process *proc) {
  uint8_t highest_needed = proc->base_priority;

  // Check all owned mutexes for highest waiter priority
  Mutex *m = proc->owned_mutexes;
  while (m) {
    if (m->wait_queue_head) {
      uint8_t waiter_prio = m->wait_queue_head->effective_priority;
      if (waiter_prio < highest_needed) {
        highest_needed = waiter_prio;
      }
    }
    m = m->next_owned;
  }

  if (highest_needed != proc->effective_priority) {
    if (proc->state == PROCESS_READY) {
      dequeue(proc);
      proc->effective_priority = highest_needed;
      enqueue(proc);
    } else {
      proc->effective_priority = highest_needed;
    }
  }
}

// ============================================================================
// Stack Guard Check (called by timer periodically)
// ============================================================================

void PriorityScheduler::check_stack_guards() {
  for (int i = 0; i < MAX_PROCESSES; i++) {
    Process *p = &process_table[i];
    if (p->state != PROCESS_UNUSED && p->stack_guard != GUARD_STACK_MAGIC) {
      // STACK OVERFLOW DETECTED
      terminal.set_color(VGA_COLOR_WHITE, VGA_COLOR_RED);
      terminal.write_string("\n[STACK OVERFLOW] Process: ");
      terminal.write_string(p->name);
      terminal.write_string("\n");
      terminal.set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);

      // Terminate the offending process
      terminate(i, 0xDEAD);
    }
  }
}

// ============================================================================
// Info Accessors
// ============================================================================

int PriorityScheduler::get_current_pid() { return current_pid; }

Process *PriorityScheduler::get_current_process() {
  if (current_pid < 0)
    return nullptr;
  return &process_table[current_pid];
}

Process *PriorityScheduler::get_process(int pid) {
  if (pid < 0 || pid >= MAX_PROCESSES)
    return nullptr;
  return &process_table[pid];
}
