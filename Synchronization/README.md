# Synchronization Mechanisms in xv6

This project explores the implementation of synchronization primitives in the xv6 operating system. The work includes practical experience modifying the xv6 kernel to support mutual exclusion, implementing locks, and analyzing cache coherence, interrupt handling, and system call counting in a multiprocessor context.

## Project Overview

The main goal is to simulate synchronization in a kernel environment that doesn't natively support user-level threading or shared memory constructs. The project explores:
- Spinlocks (busy-waiting)
- Sleep-locks (blocking)
- Ticket locks
- Read-Write locks
- Reentrant (recursive) locks
- Interrupt disabling techniques (`pushcli/popcli`, `cli/sti`)
- Cache coherence via MESI-like protocols

## Implemented Features

### 1. **Spinlock Mechanism**
- Implemented `acquire()` and `release()` for spinlocks using atomic operations.
- Used `pushcli()` and `popcli()` to prevent race conditions during critical sections.

### 2. **Sleep Locks**
- Designed `acquiresleep()` and `releasesleep()` that block the process instead of busy waiting.
- Ensures resource contention does not waste CPU cycles.

### 3. **Ticket Locks**
- FIFO-based lock that ensures fairness by serving processors in the order of arrival.
- Helps reduce starvation and improves predictability in multiprocessor scheduling.

### 4. **System Call Counter**
- Added kernel-level global counters for system calls.
- Implemented differentiated weights (coefficients) for each syscall category (e.g., `x3` for `open`, `x2` for `write`).
- Supported CPU-wise and total counting.

### 5. **Cache Coherency Awareness**
- Demonstrated how a stale L1 cache line can lead to invalid behavior across cores.
- Simulated MESI-like protocol effects on shared memory.

### 6. **Reentrant Locks**
- Created custom lock primitives that allow the same thread to acquire the lock multiple times without deadlock.
- Tracked owner and count of acquisitions to support recursive entry.

### 7. **Read-Write Locks**
- Implemented to allow multiple readers or a single writer.
- Improved performance for read-heavy scenarios.

## Testing

- Modified Makefile to support multiprocessor simulation.
- Stress-tested system calls with varying weights to ensure correct counting.
- Verified correctness of mutual exclusion using shared counters under load.

##  Notes

- Implemented locks in kernel context with attention to interrupt safety and atomicity.
- Avoided using standard libraries; all code is written in C for xv6 environment.
- Extensive kernel instrumentation and debugging were employed.