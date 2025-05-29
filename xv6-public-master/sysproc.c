#include "types.h"
#include "x86.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"

int
sys_fork(void)
{
  return fork();
}

int
sys_exit(void)
{
  exit();
  return 0;  // not reached
}

int
sys_wait(void)
{
  return wait();
}

int
sys_kill(void)
{
  int pid;

  if(argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

int
sys_getpid(void)
{
  return myproc()->pid;
}

int
sys_sbrk(void)
{
  int addr;
  int n;

  if(argint(0, &n) < 0)
    return -1;
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

int
sys_sleep(void)
{
  int n;
  uint ticks0;

  if(argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(myproc()->killed){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

int
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

int
sys_create_palindrome(void)
{
  struct proc *curproc = myproc();
  int num = curproc->tf->ecx;

  if(num < 0)
    return -1;

  return create_palindrome(num);
}

int
sys_sort_syscalls(void)
{
  int pid;

  if(argint(0, &pid) < 0)
    return -1;

  return sort_syscalls(pid);
}

int
sys_get_most_invoked_syscall(void)
{
  int pid;
  if(argint(0, &pid) < 0)
    return -1;
  
  return get_most_invoked_syscall(pid);
}

int
sys_list_all_processes(void)
{
  return list_all_processes();
}

int
sys_set_initial_burst_confidence(void)
{
  int pid, burst, confidence;

  if(argint(0, &pid) < 0 || argint(1, &burst) || argint(2, &confidence))
    return -1;

  return set_initial_burst_confidence(pid, burst, confidence);
}

int
sys_change_queue(void)
{
  int pid, target_queue;

  if(argint(0, &pid) < 0 || argint(1, &target_queue))
    return -1;

  return change_queue(pid, target_queue);
}

int
sys_print_process_information(void)
{
  return print_process_information();
}

int
sys_get_number_of_total_syscalls(void)
{
  return get_number_of_total_syscalls();
}

int
sys_reentrantlock_test(void)
{
  int count;

  if(argint(0, &count) < 0)
    return -1;

  return reentrantlock_test(count);
}