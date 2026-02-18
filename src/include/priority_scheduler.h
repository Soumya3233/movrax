#ifndef PRIORITY_SCHEDULER_H
#define PRIORITY_SCHEDULER_H

#include "timer.h"
#include "types.h"

// ============================================================================
// MOVRAX Priority-Based Preemptive Scheduler
// Military-grade scheduling with real-time guarantees
// ============================================================================

// ---------- Scheduling Policies ----------
enum SchedPolicy {
  SCHED_FIFO, // Fixed-priority, no time slice (runs until blocks/preempted)
  SCHED_RR,   // Fixed-priority with time slices (round-robin within same
              // priority)
  SCHED_EDF   // Earliest Deadline First (for periodic real-time tasks)
};

// ---------- Priority Levels (0 = highest, 255 = lowest) ----------
#define PRIORITY_LEVELS 256
#define PRIORITY_REALTIME                                                      \
  0 // 0-31:   Hard real-time (missile guidance, sensor fusion)
#define PRIORITY_CRITICAL 32 // 32-63:  Critical system tasks (crypto, watchdog)
#define PRIORITY_SYSTEM 64   // 64-95:  System services (filesystem, drivers)
#define PRIORITY_NORMAL 128  // 128-191: Normal tasks (shell, editor)
#define PRIORITY_IDLE 255    // 192-255: Background / idle

// ---------- Process Configuration ----------
#define MAX_PROCESSES 64
#define STACK_SIZE 8192       // 8KB minimum per process
#define DEFAULT_TIME_SLICE 10 // 10ms time slice for SCHED_RR (at 1000Hz timer)
#define GUARD_STACK_MAGIC 0xDEADBEEF // Stack overflow sentinel

// ---------- Process States ----------
enum ProcessState {
  PROCESS_UNUSED = 0, // Slot is free (enables process cleanup)
  PROCESS_READY,
  PROCESS_RUNNING,
  PROCESS_BLOCKED,   // Waiting on mutex/semaphore/IO
  PROCESS_SLEEPING,  // Timed sleep
  PROCESS_TERMINATED // Awaiting cleanup
};

// ---------- Forward declarations ----------
struct Mutex;

// ---------- Process Control Block ----------
struct Process {
  // Identity
  uint32_t pid;
  char name[32]; // Human-readable name for audit trail

  // CPU State
  uint32_t esp; // Saved stack pointer
  uint32_t cr3; // Page directory physical address

  // Lifecycle
  ProcessState state;
  uint32_t exit_code;

  // Scheduling
  uint8_t base_priority;      // Assigned priority (never changes)
  uint8_t effective_priority; // May be boosted by priority inheritance
  SchedPolicy policy;
  uint32_t time_slice;     // Remaining ticks for SCHED_RR
  uint32_t time_slice_max; // Reset value for time slice
  uint32_t deadline;       // Absolute deadline tick for SCHED_EDF
  uint32_t period;         // Period in ticks for periodic tasks

  // Blocking
  uint32_t sleep_until; // Wake tick for PROCESS_SLEEPING
  Mutex *blocked_on;    // Mutex this process is waiting on

  // Priority Inheritance
  Mutex *owned_mutexes; // Linked list of mutexes this process holds

  // Linked list pointers for run queue
  Process *next; // Next process in same priority queue
  Process *prev; // Previous process in same priority queue

  // Stack (must be last - guard magic placed below)
  uint32_t stack_guard; // GUARD_STACK_MAGIC - overflow detection
  uint8_t stack[STACK_SIZE] __attribute__((aligned(16)));
};

// ---------- Run Queue (O(1) Bitmap Scheduler) ----------
// Similar to Linux 2.6 O(1) scheduler
struct RunQueue {
  // Bitmap: bit N set = priority level N has at least one ready process
  // 256 bits = 8 uint32_t words
  uint32_t bitmap[PRIORITY_LEVELS / 32];

  // Per-priority linked list heads
  Process *heads[PRIORITY_LEVELS];
  Process *tails[PRIORITY_LEVELS];
};

// ---------- Scheduler Class ----------
class PriorityScheduler {
private:
  static Process process_table[MAX_PROCESSES];
  static RunQueue run_queue;
  static int current_pid;
  static int process_count;
  static bool enabled;
  static bool
      need_reschedule; // Flag set when higher-priority process becomes ready

  // Run queue operations
  static void enqueue(Process *proc);
  static void dequeue(Process *proc);
  static Process *pick_next(); // O(1) via bitmap scan

  // Internal helpers
  static Process *alloc_process();
  static void free_process(Process *proc);
  static void setup_kernel_stack(Process *proc, void (*entry_point)());
  static void setup_user_stack(Process *proc, void (*entry_point)(),
                               uint32_t user_esp);

public:
  static void initialize();

  // Process creation
  static int create_process(const char *name, void (*entry_point)(),
                            uint8_t priority, SchedPolicy policy,
                            bool kernel_mode = true, uint32_t cr3 = 0);

  // Process lifecycle
  static void terminate(int pid, uint32_t exit_code);
  static void terminate_current(uint32_t exit_code);

  // Priority inheritance (public for sync primitives)
  static void boost_priority(Process *proc, uint8_t new_priority);
  static void restore_priority(Process *proc);
  static void propagate_priority(Mutex *mutex);

  // Scheduling
  static void schedule(struct Context *ctx); // Timer-driven preemption
  static void yield();                       // Voluntary yield
  static void sleep(uint32_t ms);            // Timed sleep
  static void block(Process *proc);          // Block on mutex/semaphore
  static void unblock(Process *proc);        // Wake blocked process

  // Deadline management (SCHED_EDF)
  static void set_deadline(int pid, uint32_t deadline_ticks,
                           uint32_t period_ticks);
  static void missed_deadline_handler(Process *proc);

  // Info
  static int get_current_pid();
  static Process *get_current_process();
  static Process *get_process(int pid);
  static bool is_enabled() { return enabled; }

  // Stack guard check (called periodically by timer)
  static void check_stack_guards();
};

// ---------- Assembly Context Switch ----------
// Defined in context_switch.asm - pure assembly, compiler-independent
extern "C" void switch_context(uint32_t *old_esp, uint32_t new_esp,
                               uint32_t new_cr3);

// ---------- Interrupt Control ----------
static inline void cli() { asm volatile("cli"); }
static inline void sti() { asm volatile("sti"); }

static inline uint32_t save_flags() {
  uint32_t flags;
  asm volatile("pushf; pop %0" : "=r"(flags));
  return flags;
}

static inline void restore_flags(uint32_t flags) {
  asm volatile("push %0; popf" : : "r"(flags));
}

// Critical section helpers (save/restore interrupt state)
#define CRITICAL_ENTER()                                                       \
  uint32_t __irq_flags = save_flags();                                         \
  cli()
#define CRITICAL_EXIT() restore_flags(__irq_flags)

#endif // PRIORITY_SCHEDULER_H
