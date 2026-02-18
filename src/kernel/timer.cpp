#include "timer.h"
#include "idt.h"
#include "priority_scheduler.h"
#include "vga.h"
#include "watchdog.h"

// Static members
uint32_t Timer::tick_count = 0;

// Port I/O helpers
static inline void outb(uint16_t port, uint8_t val) {
  asm volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

// Global timer callback (referenced in isr.asm)
extern "C" void timer_callback(struct Context *ctx) { Timer::handler(ctx); }

void Timer::initialize() { set_frequency(TARGET_FREQ); }

void Timer::set_frequency(uint32_t freq) {
  uint32_t divisor = PIT_BASE_FREQ / freq;

  outb(PIT_COMMAND_REG, 0x36);
  outb(PIT_CHANNEL_0, (uint8_t)(divisor & 0xFF));
  outb(PIT_CHANNEL_0, (uint8_t)((divisor >> 8) & 0xFF));
}

void Timer::handler(struct Context *ctx) {
  tick_count++;

  // Kick watchdog (timer firing = kernel alive)
  Watchdog::kick();

  // Check watchdog (every tick = 1ms)
  Watchdog::check();

  // Periodically check stack guards (every 100ms)
  if (tick_count % 100 == 0) {
    PriorityScheduler::check_stack_guards();
  }

  // Run the priority scheduler
  PriorityScheduler::schedule(ctx);
}

uint32_t Timer::get_ticks() { return tick_count; }

void Timer::sleep(uint32_t ms) {
  uint32_t start_ticks = tick_count;
  while (tick_count < start_ticks + ms) {
    asm volatile("hlt");
  }
}
