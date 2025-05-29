# Scheduling xv6 OS

This project provides an in-depth exploration of the **xv6 operating system**, focused on understanding its **process management**, **context switching**, **interrupt handling**, and **process lifecycle**. Through careful source code analysis, function tracing, and targeted modifications, this project serves as a foundational dive into how real operating systems operate at the kernel level.

## Objectives

- Examine the internal structure of the **Process Control Block (PCB)** in xv6.
- Analyze how xv6 handles **process state transitions** and **CPU scheduling**.
- Investigate how **context switching** and **interrupts** are implemented.
- Understand kernel-level **synchronization**, especially with spinlocks and sleep/wakeup primitives.

## Key Topics Explored

### PCB vs Textbook Model
Mapped the `struct proc` fields in xv6 to the traditional PCB model taught in operating system textbooks, highlighting similarities in PID, state, context, memory size, etc.

### Process States
Described the transition between the following xv6 states:
- `UNUSED`
- `EMBRYO`
- `SLEEPING`
- `RUNNABLE`
- `RUNNING`
- `ZOMBIE`

Each state was illustrated with real function calls from xv6 like `allocproc()`, `scheduler()`, `sleep()`, `wait()`, and `exit()`.

### Context Switching
- Analyzed the `context` struct which stores only callee-saved registers.
- Explained why `eip` (instruction pointer) is stored during context switches and how `swtch()` performs stack-level context saves.
- Connected the mechanism to physical function calls like `yield()` and preemption via timer interrupts.

### Interrupt and Timer Handling
- Explored how xv6 uses the `trap.c` file to handle **hardware timer interrupts**.
- Explained the workflow from timer interrupt → `trap()` → `yield()` → `scheduler()` and how this enables preemptive multitasking.
- Illustrated how `cprintf()` logging was used to visualize this in the report.

### Sleep, Wakeup, and Orphan Handling
- Demonstrated how `sleep(chan, lock)` causes a process to give up the CPU until a wakeup is triggered.
- Investigated how **zombie** processes are collected with `wait()`.
- Studied `wait()` and `wakeup1()` logic to understand parent-child coordination.
- Illustrated orphan processes reparented to PID 1 (`initproc`), and how `wait()` still correctly collects their exit status.

## Experiments and Output
- Injected `cprintf()` statements into `scheduler()`, `sleep()`, `wakeup()`, and `trap()` to monitor live process transitions.
- Observed how `userinit`, `fork`, and `exec` lead to actual transitions in the kernel process table.
- Evaluated starvation in FIFO scheduling with repetitive `yield()` from a single process.

## How to Build & Run xv6

```bash
make qemu
```

Use `make clean` to reset the build and `make qemu-gdb` for step-through debugging.

---

> Developed for the **Operating Systems Laboratory** – Spring 2025  
> University of Tehran | Department of Electrical and Computer Engineering  