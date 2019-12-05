#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"

uint64
sys_exit(void)
{
  int n;
  if(argint(0, &n) < 0)
    return -1;
  exit(n);
  return 0;  // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return fork();
}

uint64
sys_wait(void)
{
  uint64 p;
  if(argaddr(0, &p) < 0)
    return -1;
  return wait(p);
}

uint64
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

uint64
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

uint64
sys_kill(void)
{
  int pid;

  if(argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

uint64 sys_sigalarm(void) {
  int tick;
  uint64 handler;

  if (argint(0, &tick) < 0)
    return -1;
  if (argaddr(1, &handler) < 0)
    return -1;

  myproc()->ticks = tick;
  acquire(&tickslock);
  myproc()->start_time = ticks;
  release(&tickslock);
  myproc()->handler = (void (*)())handler;

  return 0;
}

// void copytf(struct trapframe *from, struct trapframe *to) {
//   to->kernel_satp = from->kernel_satp;   // kernel page table
//   to->kernel_sp = from->kernel_sp;     // top of process's kernel stack
//   to->kernel_trap = from->kernel_trap;   // usertrap()
//   to->epc = from->epc;           // saved user program counter
//   to->kernel_hartid = from->kernel_hartid; // saved kernel tp
//   to->ra = from->ra;
//   to->sp = from->sp;
//   to->gp = from->gp;
//   to->tp = from->tp;
//   to->t0 = from->t0;
//   to->t1 = from->t1;
//   to->t2 = from->t2;
//   to->s0 = from->s0;
//   to->s1 = from->s1;
//   to->a0 = from->a0;
//   to->a1 = from->a1;
//   to->a2 = from->a2;
//   to->a3 = from->a3;
//   to->a4 = from->a4;
//   to->a5 = from->a5;
//   to->a6 = from->a6;
//   to->a7 = from->a7;
//   to->s2 = from->s2;
//   to->s3 = from->s3;
//   to->s4 = from->s4;
//   to->s5 = from->s5;
//   to->s6 = from->s6;
//   to->s7 = from->s7;
//   to->s8 = from->s8;
//   to->s9 = from->s9;
//   to->s10 = from->s10;
//   to->s11 = from->s11;
//   to->t3 = from->t3;
//   to->t4 = from->t4;
//   to->t5 = from->t5;
//   to->t6 = from->t6;
// }

uint64 sys_sigreturn(void) {
  copytf(&myproc()->savedTrapFrame, myproc()->tf);
  return 0;
}