#ifndef SYSCALLS_H
#define SYSCALLS_H

#include "types.h"
#include "timer.h" // For Context

// Syscall Numbers
#define SYS_YIELD    0
#define SYS_EXIT     1
#define SYS_PRINT    2
#define SYS_GETCHAR  3

class Syscalls {
public:
    static void initialize();
    static void handler(struct Context* ctx);
};

// Assembly handler
extern "C" void syscall_handler(struct Context* ctx);

#endif // SYSCALLS_H
