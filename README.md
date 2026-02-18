# MOVRAX OS - Military-Grade Persistent Memory Operating System
Version: 1.0

## 1. Executive Summary
MOVRAX is a specialized, real-time operating system kernel designed for high-reliability and mission-critical applications. Unlike traditional general-purpose OSes, MOVRAX employs a **persistent memory model**, treating the filesystem as a direct memory-mapped structure rather than a separate I/O abstraction.

In Version 1.0, the OS has been hardened with military-grade features, including a deterministic preemptive scheduler, strict memory protection, and continuous system integrity verification.

## 2. Key Features

### 🛡️ Reliability & Security
- **Watchdog Timer:** A hardware-backed consistency check that halts the system if the kernel becomes unresponsive for more than 5 seconds.
- **Audit Logging:** An immutable, persistent ring buffer that records critical security events (Start-up, Login, Integrity Failures).
- **Stack Protection:** Canary values (`GUARD_STACK_MAGIC`) are placed at stack boundaries and checked periodically to detect overflows.
- **W^X Memory Protection:** Enforces strict separation between writable data and executable code to prevent code-injection attacks.

### ⚡ Real-Time Performance
- **Preemptive Scheduler:** A 256-level priority scheduler ensures critical tasks (e.g., crypto, sensors) always preempt lower-priority ones immediately.
- **Deterministic Latency:** The O(1) scheduler algorithm guarantees constant-time task selection regardless of system load.
- **Priority Inheritance:** mutexes automatically boost the priority of holding tasks to prevent priority inversion deadlocks.

### 💾 Persistent Memory Filesystem
- **Zero-Copy I/O:** Files are not "loaded"; they are mapped. Reading a file is just a memory pointer dereference.
- **Integrity by Default:** Every file and directory entry is protected by a CRC32 checksum verified on access.
- **Encrypted Storage:** All persistent data is obfuscated using a stream cipher to prevent casual offline inspection.

## 3. Technical Architecture

### 3.1 Kernel Layout
The kernel is a monolithic design compiled as a 32-bit ELF binary (`kernel.bin`).

| Subsystem | Responsibility | Implementation File |
|---|---|---|
| **PMM/VMM** | Memory Allocation & Paging | `pmm.cpp`, `paging.cpp` |
| **Scheduler** | Task Scheduling & Context Switch | `priority_scheduler.cpp` |
| **Interrupts** | IDT, ISRs, IRQ Handling | `idt.cpp`, `isr.asm` |
| **Integrity** | CRC32 & Encryption | `integrity.cpp` |
| **Audit** | Security Event Logging | `audit.cpp` |
| **Shell** | Command Line Interface | `shell.cpp` |

### 3.2 Memory Map
| Address Range | Usage |
|---|---|
| `0x00000000 - 0x00100000` | **Reserved** (BIOS, IVT, Bootloader) |
| `0x00100000 - 0x00400000` | **Kernel Code & Data** (Identity Mapped) |
| `0x00400000 - 0xC0000000` | **User Heap & Stacks** |
| `0xC0000000 - 0xC0100000` | **Persistent Filesystem** (Simulated NVRAM) |

## 4. Getting Started

### 4.1 Prerequisites
You need a Linux environment (or WSL on Windows) with the following tools:
- `build-essential` (Make, GCC)
- `qemu-system-x86` (Emulator)
- `xorriso` (ISO creation)
- `i686-elf-gcc` / `g++` (Cross-compiler)

### 4.2 Building
Run the `make` command in the project root:
```bash
make
```
This produces `kernel.bin` and a bootable `mini-os.iso`.

### 4.3 Running
To boot the OS in QEMU:
```bash
make run
```

### 4.4 Debugging
To attach GDB for kernel debugging:
```bash
make debug
```
Then connect with `gdb -ex "target remote localhost:1234" kernel.bin`.

## 5. Shell Commands
Once booted, the MOVRAX shell provides the following capabilities:

| Command | Description |
|---|---|
| `help` | List available commands |
| `status` | **NEW:** Show live dashboard of all system subsystems |
| `fscheck` | **NEW:** Perform full filesystem integrity verification |
| `audit` | **NEW:** Dump the security event log |
| `watchdog`| **NEW:** Show watchdog timer statistics |
| `ls` / `cd` | Navigate the persistent filesystem |
| `write` | Create a file in persistent memory |
| `cat` | Read a file's content |

## 6. License
This project is open-source software licensed under the MIT License.
