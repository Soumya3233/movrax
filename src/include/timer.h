#ifndef TIMER_H
#define TIMER_H

#include "types.h"
#include "idt.h"

// PIT (Programmable Interval Timer) Ports
#define PIT_CHANNEL_0     0x40  // Channel 0 data port (read/write)
#define PIT_CHANNEL_1     0x41  // Channel 1 data port (unused)
#define PIT_CHANNEL_2     0x42  // Channel 2 data port (speaker)
#define PIT_COMMAND_REG   0x43  // Mode/Command register (write only)

// PIT Frequencies
#define PIT_BASE_FREQ     1193182
#define TARGET_FREQ       1000  // 1000Hz = 1ms per tick

// Structure to pass register state
struct Context {
    uint32_t gs, fs, es, ds;
    uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax;
    uint32_t int_no, err_code;
    uint32_t eip, cs, eflags, useresp, ss;
};

// Timer class
class Timer {
private:
    static uint32_t tick_count;
    
public:
    static void initialize();
    static void set_frequency(uint32_t freq);
    
    // Called by ISR
    static void handler(struct Context* ctx);
    
    static uint32_t get_ticks();
    static void sleep(uint32_t ms);
};

// C-style handler for ASM
extern "C" void timer_callback(struct Context* ctx);

#endif // TIMER_H
