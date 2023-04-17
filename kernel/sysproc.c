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
  backtrace();
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

uint64
sys_sigalarm(void)
{
  int ticks;
  uint64 handler;
  struct proc *p = myproc();
  if (argint(0, &ticks) < 0) {
    return -1;
  }
  if(argaddr(1, &handler) < 0){
    return -1;
  }
  p->ticks = ticks;
  p->handler = handler;
  p->ticks_cnt = 0;
  return 0;
}

uint64
sys_sigreturn(void)
{
  struct proc *p = myproc();
  p->trapframe->epc = p->pre_epc;
  p->trapframe->a0 = p->pre_a0;
  p->trapframe->a1 = p->pre_a1;
  p->trapframe->a2 = p->pre_a2;
  p->trapframe->a3 = p->pre_a3;
  p->trapframe->a4 = p->pre_a4;
  p->trapframe->a5 = p->pre_a5;
  p->trapframe->a6 = p->pre_a6;
  p->trapframe->a7 = p->pre_a7;
  p->trapframe->gp = p->pre_gp;
  p->trapframe->ra = p->pre_ra;
  p->trapframe->s0 = p->pre_s0;
  p->trapframe->s1 = p->pre_s1;
  p->trapframe->s2 = p->pre_s2;
  p->trapframe->s3 = p->pre_s3;
  p->trapframe->s4 = p->pre_s4;
  p->trapframe->s5 = p->pre_s5;
  p->trapframe->s6 = p->pre_s6;
  p->trapframe->s7 = p->pre_s7;
  p->trapframe->s8 = p->pre_s8;
  p->trapframe->s9 = p->pre_s9;
  p->trapframe->s10 = p->pre_s10;
  p->trapframe->s11 = p->pre_s11;
  p->trapframe->sp = p->pre_sp;
  p->trapframe->t0 = p->pre_t0;
  p->trapframe->t1 = p->pre_t1;
  p->trapframe->t2 = p->pre_t2;
  p->trapframe->t3 = p->pre_t3;
  p->trapframe->t4 = p->pre_t4;
  p->trapframe->t5 = p->pre_t5;
  p->trapframe->t6 = p->pre_t6;
  p->trapframe->tp = p->pre_tp;
  p->handler_doing = 0;
  return 0;
}
