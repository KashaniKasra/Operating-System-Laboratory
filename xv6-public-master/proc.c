#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"

struct {
  struct spinlock lock;
  struct proc proc[NPROC];
} ptable;

static struct proc *initproc;

int nextpid = 1;
extern void forkret(void);
extern void trapret(void);

static void wakeup1(void *chan);

struct reentrantlock rlock;

void
pinit(void)
{
  initlock(&ptable.lock, "ptable");
}

// Must be called with interrupts disabled
int
cpuid()
{
  return mycpu()-cpus;
}

// Must be called with interrupts disabled to avoid the caller being
// rescheduled between reading lapicid and running through the loop.
struct cpu*
mycpu(void)
{
  int apicid, i;
  
  if(readeflags()&FL_IF)
    panic("mycpu called with interrupts enabled\n");
  
  apicid = lapicid();
  // APIC IDs are not guaranteed to be contiguous. Maybe we should have
  // a reverse map, or reserve a register to store &cpus[i].
  for (i = 0; i < ncpu; ++i) {
    if (cpus[i].apicid == apicid)
      return &cpus[i];
  }
  panic("unknown apicid\n");
}

// Disable interrupts so that we are not rescheduled
// while reading proc from the cpu structure
struct proc*
myproc(void)
{
  struct cpu* c;
  struct proc* p;
  pushcli();
  c = mycpu();
  p = c->proc;
  popcli();
  return p;
}

//PAGEBREAK: 32
// Look in the process table for an UNUSED proc.
// If found, change state to EMBRYO and initialize
// state required to run in the kernel.
// Otherwise return 0.
static struct proc*
allocproc(void)
{
  struct proc* p;
  char* sp;

  acquire(&ptable.lock);

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == UNUSED)
      goto found;

  release(&ptable.lock);
  return 0;

found:
  p->state = EMBRYO;
  p->pid = nextpid++;

  for(int i = 0; i < MAX_SYSCALLS; i++){
    p->used_syscalls[i] = 0;
  }

  release(&ptable.lock);

  // Allocate kernel stack.
  if((p->kstack = kalloc()) == 0){
    p->state = UNUSED;
    return 0;
  }
  sp = p->kstack + KSTACKSIZE;

  // Leave room for trap frame.
  sp -= sizeof *p->tf;
  p->tf = (struct trapframe*)sp;

  // Set up new context to start executing at forkret,
  // which returns to trapret.
  sp -= 4;
  *(uint*)sp = (uint)trapret;

  sp -= sizeof *p->context;
  p->context = (struct context*)sp;
  memset(p->context, 0, sizeof *p->context);
  p->context->eip = (uint)forkret;

  struct timeInfo init_ti = {.queue = FCFS, .ticks_used = 0, .burst_time = DEFAULT_BURST_TIME, .creation_time = ticks, .enter_queue_time = ticks,
                             .last_run_time = 0, .confidence = DEFAULT_CONFIDENCE};
  p->ti = init_ti;

  return p;
}

//PAGEBREAK: 32
// Set up first user process.
void
userinit(void)
{
  struct proc* p;
  extern char _binary_initcode_start[], _binary_initcode_size[];

  p = allocproc();
  
  initproc = p;
  if((p->pgdir = setupkvm()) == 0)
    panic("userinit: out of memory?");
  inituvm(p->pgdir, _binary_initcode_start, (int)_binary_initcode_size);
  p->sz = PGSIZE;
  memset(p->tf, 0, sizeof(*p->tf));
  p->tf->cs = (SEG_UCODE << 3) | DPL_USER;
  p->tf->ds = (SEG_UDATA << 3) | DPL_USER;
  p->tf->es = p->tf->ds;
  p->tf->ss = p->tf->ds;
  p->tf->eflags = FL_IF;
  p->tf->esp = PGSIZE;
  p->tf->eip = 0;  // beginning of initcode.S

  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei("/");

  // this assignment to p->state lets other cores
  // run this process. the acquire forces the above
  // writes to be visible, and the lock is also needed
  // because the assignment might not be atomic.
  acquire(&ptable.lock);

  p->state = RUNNABLE;
  int pid = p->pid;

  release(&ptable.lock);

  change_queue(pid, RR);
}

// Grow current process's memory by n bytes.
// Return 0 on success, -1 on failure.
int
growproc(int n)
{
  uint sz;
  struct proc* curproc = myproc();

  sz = curproc->sz;
  if(n > 0){
    if((sz = allocuvm(curproc->pgdir, sz, sz + n)) == 0)
      return -1;
  } else if(n < 0){
    if((sz = deallocuvm(curproc->pgdir, sz, sz + n)) == 0)
      return -1;
  }
  curproc->sz = sz;
  switchuvm(curproc);
  return 0;
}

// Create a new process copying p as the parent.
// Sets up stack to return as if from system call.
// Caller must set state of returned proc to RUNNABLE.
int
fork(void)
{
  int i, pid;
  struct proc* np;
  struct proc* curproc = myproc();

  // Allocate process.
  if((np = allocproc()) == 0){
    return -1;
  }

  // Copy process state from proc.
  if((np->pgdir = copyuvm(curproc->pgdir, curproc->sz)) == 0){
    kfree(np->kstack);
    np->kstack = 0;
    np->state = UNUSED;
    return -1;
  }
  np->sz = curproc->sz;
  np->parent = curproc;
  *np->tf = *curproc->tf;

  // Clear %eax so that fork returns 0 in the child.
  np->tf->eax = 0;

  for(i = 0; i < NOFILE; i++)
    if(curproc->ofile[i])
      np->ofile[i] = filedup(curproc->ofile[i]);
  np->cwd = idup(curproc->cwd);

  safestrcpy(np->name, curproc->name, sizeof(curproc->name));

  pid = np->pid;

  acquire(&ptable.lock);

  np->state = RUNNABLE;

  release(&ptable.lock);

  if(pid == 2)
    change_queue(pid, RR);

  return pid;
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait() to find out it exited.
void
exit(void)
{
  struct proc* curproc = myproc();
  struct proc* p;
  int fd;

  if(curproc == initproc)
    panic("init exiting");

  // Close all open files.
  for(fd = 0; fd < NOFILE; fd++){
    if(curproc->ofile[fd]){
      fileclose(curproc->ofile[fd]);
      curproc->ofile[fd] = 0;
    }
  }

  begin_op();
  iput(curproc->cwd);
  end_op();
  curproc->cwd = 0;

  acquire(&ptable.lock);

  // Parent might be sleeping in wait().
  wakeup1(curproc->parent);

  // Pass abandoned children to init.
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->parent == curproc){
      p->parent = initproc;
      if(p->state == ZOMBIE)
        wakeup1(initproc);
    }
  }

  // Jump into the scheduler, never to return.
  curproc->state = ZOMBIE;
  sched();
  panic("zombie exit");
}

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int
wait(void)
{
  struct proc* p;
  int havekids, pid;
  struct proc* curproc = myproc();
  
  acquire(&ptable.lock);
  for(;;){
    // Scan through table looking for exited children.
    havekids = 0;
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->parent != curproc)
        continue;
      havekids = 1;
      if(p->state == ZOMBIE){
        // Found one.
        pid = p->pid;
        kfree(p->kstack);
        p->kstack = 0;
        freevm(p->pgdir);
        p->pid = 0;
        p->parent = 0;
        p->name[0] = 0;
        p->killed = 0;
        p->state = UNUSED;
        release(&ptable.lock);
        return pid;
      }
    }

    // No point waiting if we don't have any children.
    if(!havekids || curproc->killed){
      release(&ptable.lock);
      return -1;
    }

    // Wait for children to exit.  (See wakeup1 call in proc_exit.)
    sleep(curproc, &ptable.lock);  //DOC: wait-sleep
  }
}

//PAGEBREAK: 42
// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run
//  - swtch to start running that process
//  - eventually that process transfers control
//      via swtch back to the scheduler.

void
scheduler(void)
{
  struct proc* p;
  struct proc* last_executed_proc = &ptable.proc[NPROC - 1];
  struct cpu* c = mycpu();
  c->proc = 0;
  
  for(;;) {
    // Enable interrupts on this processor.
    sti();

    // Loop over process table looking for process to run.
    acquire(&ptable.lock);

    if(mycpu()->RR_remain_time > 0) {
      p = round_robin(last_executed_proc);

      if(p != 0) {
        last_executed_proc = p;
        run_process_on_cpu(p, c);

        continue;
      }
    }

    if(mycpu()->SJF_remain_time > 0) {
      p = shortest_job_first();

      if(p != 0) {
        run_process_on_cpu(p, c);

        continue;
      }
    }

    if(mycpu()->FCFS_remain_time > 0) {
      p = first_come_first_service();

      if(p != 0) {
        run_process_on_cpu(p, c);

        continue;
      }
    }

    mycpu()->RR_remain_time = RR_WEIGHT * TIME_SLICE_UNIT / SYS_TICK;
    mycpu()->SJF_remain_time = SJF_WEIGHT * TIME_SLICE_UNIT / SYS_TICK;
    mycpu()->FCFS_remain_time = FCFS_WEIGHT * TIME_SLICE_UNIT / SYS_TICK;

    release(&ptable.lock);

  }
}

void
sched(void)
{
  int intena;
  struct proc* p = myproc();

  if(!holding(&ptable.lock))
    panic("sched ptable.lock");
  if(mycpu()->ncli != 1)
    panic("sched locks");
  if(p->state == RUNNING)
    panic("sched running");
  if(readeflags()&FL_IF)
    panic("sched interruptible");
  intena = mycpu()->intena;
  swtch(&p->context, mycpu()->scheduler);
  mycpu()->intena = intena;
}

// Give up the CPU for one scheduling round.
void
yield(void)
{
  acquire(&ptable.lock);  //DOC: yieldlock
  myproc()->state = RUNNABLE;
  sched();
  release(&ptable.lock);
}

// A fork child's very first scheduling by scheduler()
// will swtch here.  "Return" to user space.
void
forkret(void)
{
  static int first = 1;
  // Still holding ptable.lock from scheduler.
  release(&ptable.lock);

  if (first) {
    // Some initialization functions must be run in the context
    // of a regular process (e.g., they call sleep), and thus cannot
    // be run from main().
    first = 0;
    iinit(ROOTDEV);
    initlog(ROOTDEV);
  }

  // Return to "caller", actually trapret (see allocproc).
}

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
void
sleep(void* chan, struct spinlock* lk)
{
  struct proc* p = myproc();
  
  if(p == 0)
    panic("sleep");

  if(lk == 0)
    panic("sleep without lk");

  // Must acquire ptable.lock in order to
  // change p->state and then call sched.
  // Once we hold ptable.lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup runs with ptable.lock locked),
  // so it's okay to release lk.
  if(lk != &ptable.lock){  //DOC: sleeplock0
    acquire(&ptable.lock);  //DOC: sleeplock1
    release(lk);
  }
  // Go to sleep.
  p->chan = chan;
  p->state = SLEEPING;

  sched();

  // Tidy up.
  p->chan = 0;

  // Reacquire original lock.
  if(lk != &ptable.lock){  //DOC: sleeplock2
    release(&ptable.lock);
    acquire(lk);
  }
}

//PAGEBREAK!
// Wake up all processes sleeping on chan.
// The ptable lock must be held.
static void
wakeup1(void* chan)
{
  struct proc* p;

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == SLEEPING && p->chan == chan)
      p->state = RUNNABLE;
}

// Wake up all processes sleeping on chan.
void
wakeup(void* chan)
{
  acquire(&ptable.lock);
  wakeup1(chan);
  release(&ptable.lock);
}

// Kill the process with the given pid.
// Process won't exit until it returns
// to user space (see trap in trap.c).
int
kill(int pid)
{
  struct proc* p;

  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->pid == pid){
      p->killed = 1;
      // Wake process from sleep if necessary.
      if(p->state == SLEEPING)
        p->state = RUNNABLE;
      release(&ptable.lock);
      return 0;
    }
  }
  release(&ptable.lock);
  return -1;
}

//PAGEBREAK: 36
// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.
void
procdump(void)
{
  static char* states[] = {
  [UNUSED]    "unused",
  [EMBRYO]    "embryo",
  [SLEEPING]  "sleep ",
  [RUNNABLE]  "runble",
  [RUNNING]   "run   ",
  [ZOMBIE]    "zombie"
  };
  int i;
  struct proc* p;
  char* state;
  uint pc[10];

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->state == UNUSED)
      continue;
    if(p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";
    cprintf("%d %s %s", p->pid, state, p->name);
    if(p->state == SLEEPING){
      getcallerpcs((uint*)p->context->ebp+2, pc);
      for(i=0; i<10 && pc[i] != 0; i++)
        cprintf(" %p", pc[i]);
    }
    cprintf("\n");
  }
}

struct proc*
get_proc_by_pid(int pid)
{ 
    struct proc* p;

    acquire(&ptable.lock);
    for (p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
        if (p->pid == pid) {
            release(&ptable.lock);
            return p;
        }
    }
    release(&ptable.lock);

    return 0;  // Return 0 if the process with the given PID was not found
}

int
get_total_syscalls(int pid)
{
  struct proc* p = get_proc_by_pid(pid);
  int num_of_syscalls = 0;
  
  if(p == 0)
    return -1;

  for(int i = 0; i < MAX_SYSCALLS; i++){
    num_of_syscalls += p->used_syscalls[i];
  }

  return num_of_syscalls;
}


int
create_palindrome(int num)
{
  int number_of_digits = 0, temp_num = num;
  char num_str[128], palindrome_str[256];
  
  if(num == 0)
  {
    num_str[number_of_digits] = (num % 10) + '0';
    number_of_digits ++;
  }

  while(num > 0)
  {
    num_str[number_of_digits] = (num % 10) + '0';
    number_of_digits ++;
    num = num / 10;
  }

  num_str[number_of_digits] = '\0';

  for(int i = 0; i < number_of_digits; i++)
  {
    palindrome_str[i] = num_str[number_of_digits - i - 1];
    palindrome_str[number_of_digits + i] = num_str[i];
  }

  palindrome_str[2 * number_of_digits] = '\0';
  
  cprintf("Palindrome of %d is %s\n", temp_num, palindrome_str);

  return 0;
}

int
sort_syscalls(int pid)
{
  struct proc* p = get_proc_by_pid(pid);

  if(p == 0)
    return -1;

  for(int i = 0; i < MAX_SYSCALLS; i++){
    cprintf("\tSystem call number %d: %d usage\n", i + 1, p->used_syscalls[i]);
  }

  return 0;
}

int
get_most_invoked_syscall(int pid)
{
  int max_count = 0, index_max = 0;
  struct proc *p = get_proc_by_pid(pid);

  if(p == 0)
    return -1;

  for(int i = 0; i < MAX_SYSCALLS; i++){
    if(p->used_syscalls[i] > max_count){
      max_count = p->used_syscalls[i];
      index_max = i;
    }
  }

  cprintf("The most invoked system call is system call number %d with %d usage\n", index_max + 1, max_count);

  return 0;
}

int
list_all_processes(void)
{
  int flag = 0;

  for (struct proc *p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
    if (p->state != UNUSED) {  // Check if the process is active
      cprintf("Process with id %d and name %s has totally %d system calls\n", p->pid, p->name, get_total_syscalls(p->pid));
      flag = 1;
    }
  }

  if(!flag)
    return -1;
  
  return 0;
}

int
create_random_number(int seed)
{
  return ticks % seed;
}

struct proc*
round_robin(struct proc* last_executed_proc)
{
  int count = 0;

  for(struct proc* p = last_executed_proc; count < sizeof(ptable.proc); p++, count++) {
    if(p >= &ptable.proc[NPROC])
      p = ptable.proc;

    if((p->state == RUNNABLE) && (p->ti.queue == RR))
      return p;
  }

  return 0;
}

struct proc*
shortest_job_first()
{
  struct proc* shortest_time_proc = 0;

  for(struct proc* p = ptable.proc; p < &ptable.proc[NPROC]; p++)
  {
    if((p->state != RUNNABLE) || (p->ti.queue != SJF))
      continue;

    if(shortest_time_proc == 0)
      shortest_time_proc = p;

    if(p->ti.burst_time < shortest_time_proc->ti.burst_time)
      if(p->ti.confidence > create_random_number(100))
        shortest_time_proc = p;
  }

  return shortest_time_proc;
}

struct proc*
first_come_first_service()
{
  struct proc* earliest_entered_time_proc = 0;

  for(struct proc* p = ptable.proc; p < &ptable.proc[NPROC]; p++)
  {
    if((p->state != RUNNABLE) || (p->ti.queue != FCFS))
      continue;

    if(earliest_entered_time_proc == 0)
      earliest_entered_time_proc = p;

    if(p->ti.enter_queue_time < earliest_entered_time_proc->ti.enter_queue_time)
      earliest_entered_time_proc = p;
  }

  return earliest_entered_time_proc;
}

void
aging()
{
  acquire(&ptable.lock);

  for (struct proc* p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
    if (p->state != RUNNABLE || p->ti.queue == RR)
      continue;

    if ((ticks - p->ti.last_run_time > STARVATION_BOUNDRY) && (ticks - p->ti.enter_queue_time > STARVATION_BOUNDRY)) {
        release(&ptable.lock);

        if(p->ti.queue == SJF)
          change_queue(p->pid, RR);
        else if(p->ti.queue == FCFS)
          change_queue(p->pid, SJF);

        //cprintf("pid: %d starved!\n", p->pid);

        acquire(&ptable.lock);
    }  
  }

  release(&ptable.lock);
}

void
run_process_on_cpu(struct proc* p, struct cpu* c)
{
    c->proc = p;

    switchuvm(p);

    p->state = RUNNING;
    //p->ti.last_run_time = ticks;

    swtch(&(c->scheduler), p->context);

    switchkvm();
    
    c->proc = 0;

    release(&ptable.lock);
}

int
set_initial_burst_confidence(int pid, int burst, int confidence)
{
  struct proc* p = get_proc_by_pid(pid);

  p->ti.burst_time = burst;
  p->ti.confidence = confidence;

  cprintf("pid: %d new_burst: %d new_confidence: %d\n", pid, p->ti.burst_time, p->ti.confidence);

  return 0;
}

int
change_queue(int pid, int target_queue)
{
  struct proc *p = get_proc_by_pid(pid);

  acquire(&ptable.lock);

  p->ti.queue = target_queue;
  p->ti.enter_queue_time = ticks;

  release(&ptable.lock);

  return 0;
}

int
print_process_information()
{
  static char* states[] = {
      [UNUSED] "unused",
      [EMBRYO] "embryo",
      [SLEEPING] "sleeping",
      [RUNNABLE] "runnable",
      [RUNNING] "running",
      [ZOMBIE] "zombie"};

  cprintf("process_name / PID / state / queue / burst_time / waiting_time / arrival / consecutive run / confidence\n\n");

  for (struct proc* p = ptable.proc; p < &ptable.proc[NPROC]; p++)
  {
    if (p->state == UNUSED)
      continue;

    const char* state;
    if (p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "unknown state";

    cprintf("%s / ", p->name);
    cprintf("%d / ", p->pid);
    cprintf("%s / ", state);
    cprintf("%d / ", p->ti.queue);
    cprintf("%d / ", (int)p->ti.burst_time);
    cprintf("%d / ", ticks - p->ti.last_run_time);
    cprintf("%d / ", p->ti.enter_queue_time);
    cprintf("%d / ", p->ti.last_run_time);
    cprintf("%d", (int)p->ti.confidence);
    cprintf("\n");
  }

  return 0;
}

int
get_number_of_total_syscalls()
{
  int count = 0;
  for(int i = 0; i < NCPU; i++) {
    if(cpus[i].number_of_syscalls != 0) {
      count += cpus[i].number_of_syscalls;
      cprintf("number of syscalls for cpu {%d} is : %d\n", i, cpus[i].number_of_syscalls);
    }
  }

  cprintf("number of total syscalls for all cpus is (sum of cpus): %d\n", count);

  cprintf("number of total syscalls for all cpus is (global) : %d\n", total_number_of_syscalls);

  return 0;
}

void
initreentrantlock(char *name)
{
  initlock(&rlock.lock, name);
  rlock.owner = 0;
  rlock.recursion = 0;
}

void
acquirereentrantlock()
{
  if(rlock.owner == myproc()) {
    rlock.recursion ++;
  }
  else {
    acquire(&rlock.lock);
    rlock.owner = myproc();
    rlock.recursion = 1;
  }
}

void
releasereentrantlock()
{
  if(rlock.owner != myproc())
    panic("reentrant release");

  rlock.recursion --;

  if(rlock.recursion == 0) {
    cprintf("ttt\n");
    release(&rlock.lock);
    rlock.owner = 0;
    cprintf("qqqq\n");
  }
}

void rinit()
{
  initreentrantlock("my lock");
}

int
reentrantlock_test(int count)
{
  cprintf("pid %d\n", myproc()->pid);

  if(count == 5) {
      cprintf("A recursive function terminated without a deadlock!\n");
      return;
  }

  acquirereentrantlock();
  count ++;
  cprintf("%daaaa\n", count);
  reentrantlock_test(count);
  cprintf("%dmmmmm\n", count);
  releasereentrantlock();
  cprintf("done\n");

  return 0;
}