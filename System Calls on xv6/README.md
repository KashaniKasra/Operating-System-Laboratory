
# Custom System Calls in xv6

This project extends the **xv6 UNIX-like operating system** by implementing four custom system calls to interact with the file system and process structures. These system calls enhance file management, monitor process activity, and demonstrate syscall tracking inside the xv6 kernel.

## Objectives

- Learn how system calls are implemented and invoked in xv6.
- Manipulate kernel-level data structures such as the process table.
- Implement basic file operations like move and syscall statistics tracking.

## System Calls Implemented

### 1. `int move_file(const char* src_file, const char* dest_dir)`
- Copies a file to a new directory and deletes the original.
- Returns `0` on success, `-1` on error (e.g., missing file, invalid path).

### 2. `int sort_syscalls(int pid)`
- Sorts and displays all system calls made by a process with the given `pid`, sorted by syscall number.
- Returns `0` on success, `-1` if the process doesn't exist or has no tracked calls.

### 3. `int get_most_invoked_syscall(int pid)`
- Returns the syscall number most frequently used by a process.
- Returns `-1` if no data exists or PID is invalid.

### 4. `int list_all_processes()`
- Lists all active processes with their PID and number of syscalls invoked.
- Displays result directly to the console.

## Kernel Modifications

- `proc.h`: Added syscall tracking arrays in the `struct proc`.
- `syscall.c`: Added handler functions for new system calls.
- `syscall.h`: Registered syscall numbers.
- `usys.S`: Added user-level syscall entry points.
- `user.h`: Declared syscall prototypes for user programs.
- `trap.c`: Modified to increment per-process syscall counters.

## Testing

Includes a test program `syscall_test.c` which:
- Creates files and directories.
- Moves files using `move_file`.
- Forks processes to generate syscall traffic.
- Displays syscall stats using `sort_syscalls`, `get_most_invoked_syscall`, and `list_all_processes`.

## Build & Run

### Build xv6:

```bash
make qemu
```

### Clean Build:

```bash
make clean
make qemu
```

### Test:

```bash
$ syscall_test
```

> Developed for the **Operating Systems Laboratory** – Spring 2025  
> University of Tehran | Department of Electrical and Computer Engineering  