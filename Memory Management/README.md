# xv6 Shared Memory Extension

This project extends the xv6 operating system to support **shared memory** between user processes.

## Features Implemented

- **Shared Memory Allocation and Mapping:**
  - Implemented system call `open_sharedmem(id)`:
    - If a shared memory page with the given ID exists, maps it to the current process and increments the reference count.
    - Otherwise, allocates a new page, maps it, and initializes its reference count.
  - Implemented system call `close_sharedmem(id)`:
    - Decrements the reference count.
    - If the reference count becomes 0, unmaps the page and deallocates it.

- **Shared Page Metadata Management:**
  - A global table (`sharedMemory`) with `MAX_SHARED_PAGES` entries tracks shared pages.
  - Each entry contains:
    - `id`: Unique page identifier.
    - `frame`: Physical memory pointer to the page.
    - `ref_count`: Reference count indicating how many processes are using the page.
    - Access is synchronized using a kernel-level lock.

- **Initialization:**
  - Kernel initializes all reference counts to 0 using `init_sharedmem()`.

- **User-Level Test Program:**
  - Demonstrates shared memory usage with a factorial calculator:
    - Parent process allocates shared page and writes `1`.
    - Spawns 5 child processes; each updates the page with its `(pid+1)!` using shared memory.
    - Parent verifies final result is 120 (`5!`).

## Files and Components

- `kernel/sysproc.c` – Contains logic for `open_sharedmem` and `close_sharedmem`.
- `kernel/sharedmem.c` – Contains shared memory table and helper functions.
- `user/sharedmem_test.c` – User-level program to validate shared memory logic.

## Testing & Validation

- Correctness validated by ensuring factorial values accumulate correctly.
- Concurrency tested via simultaneous updates from multiple child processes.
- Output confirms expected value of factorial with/without explicit synchronization.

## Limitations & Notes

- No explicit locking mechanism for user-level race conditions.
- Primarily intended as an educational OS-level memory management exercise.