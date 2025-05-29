#include "syscall.h"
#include "traps.h"

#define SYSCALL(name) \
  .globl name; \
  name: \
    movl $SYS_ ## name, %eax; \
    int $T_SYSCALL; \
    ret

SYSCALL(fork)
SYSCALL(exit)
SYSCALL(wait)
SYSCALL(pipe)
SYSCALL(read)
SYSCALL(write)
SYSCALL(close)
SYSCALL(kill)
SYSCALL(exec)
SYSCALL(open)
SYSCALL(mknod)
SYSCALL(unlink)
SYSCALL(fstat)
SYSCALL(link)
SYSCALL(mkdir)
SYSCALL(chdir)
SYSCALL(dup)
SYSCALL(getpid)
SYSCALL(sbrk)
SYSCALL(sleep)
SYSCALL(uptime)
SYSCALL(create_palindrome)
SYSCALL(move_file)
SYSCALL(sort_syscalls)
SYSCALL(get_most_invoked_syscall)
SYSCALL(list_all_processes)
SYSCALL(set_initial_burst_confidence)
SYSCALL(change_queue)
SYSCALL(print_process_information)
SYSCALL(get_number_of_total_syscalls)
SYSCALL(reentrantlock_test)