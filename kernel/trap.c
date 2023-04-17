#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"

struct spinlock tickslock;
uint ticks;

extern char trampoline[], uservec[], userret[];

// in kernelvec.S, calls kerneltrap().
void kernelvec();

extern int devintr();

void
trapinit(void)
{
  initlock(&tickslock, "time");
}

// set up to take exceptions and traps while in the kernel.
void
trapinithart(void)
{
  w_stvec((uint64)kernelvec);
}

void 
store_register(void)
{
  struct proc *p = myproc();
  p->pre_epc = p->trapframe->epc;
  p->pre_a0 = p->trapframe->a0;
  p->pre_a1 = p->trapframe->a1;
  p->pre_a2 = p->trapframe->a2;
  p->pre_a3 = p->trapframe->a3;
  p->pre_a4 = p->trapframe->a4;
  p->pre_a5 = p->trapframe->a5;
  p->pre_a6 = p->trapframe->a6;
  p->pre_a7 = p->trapframe->a7;
  p->pre_gp = p->trapframe->gp;
  p->pre_ra = p->trapframe->ra;
  p->pre_s0 = p->trapframe->s0;
  p->pre_s1 = p->trapframe->s1;
  p->pre_s2 = p->trapframe->s2;
  p->pre_s3 = p->trapframe->s3;
  p->pre_s4 = p->trapframe->s4;
  p->pre_s5 = p->trapframe->s5;
  p->pre_s6 = p->trapframe->s6;
  p->pre_s7 = p->trapframe->s7;
  p->pre_s8 = p->trapframe->s8;
  p->pre_s9 = p->trapframe->s9;
  p->pre_s10 = p->trapframe->s10;
  p->pre_s11 = p->trapframe->s11;
  p->pre_sp = p->trapframe->sp;
  p->pre_t0 = p->trapframe->t0;
  p->pre_t1 = p->trapframe->t1;
  p->pre_t2 = p->trapframe->t2;
  p->pre_t3 = p->trapframe->t3;
  p->pre_t4 = p->trapframe->t4;
  p->pre_t5 = p->trapframe->t5;
  p->pre_t6 = p->trapframe->t6;
  p->pre_tp = p->trapframe->tp;
}
//
// handle an interrupt, exception, or system call from user space.
// called from trampoline.S
// 确定陷阱的原因，处理并返回
void
usertrap(void)
{
  int which_dev = 0;

  if((r_sstatus() & SSTATUS_SPP) != 0)
    panic("usertrap: not from user mode");

  // send interrupts and exceptions to kerneltrap(),
  // since we're now in the kernel.改变stvec，这样内核中的陷阱将由kernelvec处理
  w_stvec((uint64)kernelvec);

  struct proc *p = myproc();
  
  // save user program counter.保存的用户程序计数器
  p->trapframe->epc = r_sepc();
  
  if(r_scause() == 8){
    // system call

    if(p->killed)
      exit(-1);

    // sepc points to the ecall instruction,
    // but we want to return to the next instruction.
    p->trapframe->epc += 4;

    // an interrupt will change sstatus &c registers,
    // so don't enable until done with those registers.
    intr_on();

    syscall();
  } else if((which_dev = devintr()) != 0){
    // ok
  } else {
    printf("usertrap(): unexpected scause %p pid=%d\n", r_scause(), p->pid);
    printf("            sepc=%p stval=%p\n", r_sepc(), r_stval());
    p->killed = 1;
  }

  if(p->killed)
    exit(-1);

  // give up the CPU if this is a timer interrupt.
  if(which_dev == 2){
    if (p->ticks > 0) {
      ++p->ticks_cnt;
      if (p->ticks_cnt > p->ticks && p->handler_doing == 0) {
        p->ticks_cnt = 0;
        store_register();
        // 这样做的目的是为了在恢复到user mode的时候，程序可以运行handler
        //因为恢复到user mode的时候，epc会赋给pc，而程序正是根据pc来执行的，所以才需要保存epc的问题，来恢复现场
        p->trapframe->epc = p->handler; 
        p->handler_doing = 1;
      }
    }
    yield();
  }

  usertrapret();
}

//
// return to user space
//
void
usertrapret(void)
{
  struct proc *p = myproc();

  // we're about to switch the destination of traps from
  // kerneltrap() to usertrap(), so turn off interrupts until
  // we're back in user space, where usertrap() is correct.
  intr_off();

  // send syscalls, interrupts, and exceptions to trampoline.S
  w_stvec(TRAMPOLINE + (uservec - trampoline));

  // set up trapframe values that uservec will need when
  // the process next re-enters the kernel.
  p->trapframe->kernel_satp = r_satp();         // kernel page table
  p->trapframe->kernel_sp = p->kstack + PGSIZE; // process's kernel stack
  p->trapframe->kernel_trap = (uint64)usertrap;
  p->trapframe->kernel_hartid = r_tp();         // hartid for cpuid()

  // set up the registers that trampoline.S's sret will use
  // to get to user space.
  
  // set S Previous Privilege mode to User.
  unsigned long x = r_sstatus();
  x &= ~SSTATUS_SPP; // clear SPP to 0 for user mode
  x |= SSTATUS_SPIE; // enable interrupts in user mode
  w_sstatus(x);

  // set S Exception Program Counter to the saved user pc.
  w_sepc(p->trapframe->epc);

  // tell trampoline.S the user page table to switch to.
  uint64 satp = MAKE_SATP(p->pagetable);

  // jump to trampoline.S at the top of memory, which 
  // switches to the user page table, restores user registers,
  // and switches to user mode with sret.
  uint64 fn = TRAMPOLINE + (userret - trampoline);
  ((void (*)(uint64,uint64))fn)(TRAPFRAME, satp);
}

// interrupts and exceptions from kernel code go here via kernelvec,
// on whatever the current kernel stack is.为两种类型的陷阱做好了准备：设备中断和异常。
void 
kerneltrap()
{
  int which_dev = 0;
  uint64 sepc = r_sepc();
  uint64 sstatus = r_sstatus();
  uint64 scause = r_scause();
  
  if((sstatus & SSTATUS_SPP) == 0)
    panic("kerneltrap: not from supervisor mode");
  if(intr_get() != 0)
    panic("kerneltrap: interrupts enabled");

  if((which_dev = devintr()) == 0){
    printf("scause %p\n", scause);
    printf("sepc=%p stval=%p\n", r_sepc(), r_stval());
    panic("kerneltrap");
  }

  // give up the CPU if this is a timer interrupt.
  if(which_dev == 2 && myproc() != 0 && myproc()->state == RUNNING)
    yield();

  // the yield() may have caused some traps to occur,
  // so restore trap registers for use by kernelvec.S's sepc instruction.
  w_sepc(sepc);
  w_sstatus(sstatus);
}

void
clockintr()
{
  acquire(&tickslock);
  ticks++;
  wakeup(&ticks);
  release(&tickslock);
}

// check if it's an external interrupt or software interrupt,
// and handle it.
// returns 2 if timer interrupt,
// 1 if other device,
// 0 if not recognized.
int
devintr()
{
  uint64 scause = r_scause();

  if((scause & 0x8000000000000000L) &&
     (scause & 0xff) == 9){
    // this is a supervisor external interrupt, via PLIC.

    // irq indicates which device interrupted.
    int irq = plic_claim();

    if(irq == UART0_IRQ){
      uartintr();
    } else if(irq == VIRTIO0_IRQ){
      virtio_disk_intr();
    } else if(irq){
      printf("unexpected interrupt irq=%d\n", irq);
    }

    // the PLIC allows each device to raise at most one
    // interrupt at a time; tell the PLIC the device is
    // now allowed to interrupt again.
    if(irq)
      plic_complete(irq);

    return 1;
  } else if(scause == 0x8000000000000001L){
    // software interrupt from a machine-mode timer interrupt,
    // forwarded by timervec in kernelvec.S.

    if(cpuid() == 0){
      clockintr();
    }
    
    // acknowledge the software interrupt by clearing
    // the SSIP bit in sip.
    w_sip(r_sip() & ~2);

    return 2;
  } else {
    return 0;
  }
}

