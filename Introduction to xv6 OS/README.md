
# Introduction to xv6 Operating System

This project involves exploring and extending the **xv6 UNIX-like operating system** to gain practical experience in kernel-level development. Students modify the kernel, add new functionalities, and interact directly with low-level system components such as processes, memory, and synchronization primitives.

## Objectives

- Understand the internal workings of a UNIX-style operating system through hands-on modification.
- Extend and test custom **system calls**, memory management techniques, and **scheduling algorithms**.
- Implement synchronization primitives and explore process lifecycle at kernel level.

## Project Topics Covered

1. **Shell and Terminal Extensions**  
   Added new shell commands, improved terminal input behavior, and implemented user-level process utilities.

2. **System Call Implementation**  
   Created and tested new system calls by modifying `usys.S`, `syscall.c`, `proc.c`, and `sysproc.c`.

3. **Scheduling Algorithms**  
   Implemented alternative CPU schedulers to replace xv6’s default round-robin policy, comparing turnaround and waiting time.

4. **Synchronization Mechanisms**  
   Explored race conditions and implemented **mutex locks** and **semaphores** for safe concurrent process handling.

5. **Memory Management**  
   Investigated paging, page faults, and implemented custom memory access logging.

## Setup Instructions

### Build

```bash
make qemu
```

### Clean and Rebuild

```bash
make clean
make qemu
```

### Debugging

```bash
make qemu-gdb
```

Use with `gdb` for step-by-step debugging.

## 🧪 Test Examples

- `test_syscall`: Verifies added system call execution and return values.
- `sched_test`: Simulates multiple processes under different schedulers.
- `lock_test`: Spawns concurrent threads to evaluate synchronization logic.

> Developed for the **Operating Systems Lab** course – Spring 2025  
> University of Tehran | Department of Electrical and Computer Engineering  