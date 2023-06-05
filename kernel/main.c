#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "defs.h"

volatile static int started = 0;

// start() jumps here in supervisor mode on all CPUs.
// 在启动过程中控制其他模块初始化
void
main()
{ // 只有cpuid为0的那个内核线程才会做这么多的初始化，除了kvminithart，trapinithart，plicinithart三个会每个cpu都做
  // 当CPU0 做完全部的初始化后，其他CPU才会继续运行，通过改变started(是一个只从内存读取的变量)这个变量来通知其他CPU
  if(cpuid() == 0){
    consoleinit();
    printfinit();
    printf("\n");
    printf("xv6 kernel is booting\n");
    printf("\n");
    kinit();         // physical page allocator 初始化分配器
    kvminit();       // create kernel page table 创建内核的页表
    kvminithart();   // turn on paging 给MMU安装内核页表，这里地址转换就会被启用了
    procinit();      // process table 每个进程分配一个内核栈
    trapinit();      // trap vectors
    trapinithart();  // install kernel trap vector 安装内核trap vec
    plicinit();      // set up interrupt controller // 设置中断控制器
    plicinithart();  // ask PLIC for device interrupts // 告诉PLIC该CPU对设备中断感兴趣
    binit();         // buffer cache 
    iinit();         // inode cache
    fileinit();      // file table
    virtio_disk_init(); // emulated hard disk
    userinit();      // first user process
    __sync_synchronize(); // 告诉编译器不能改变指令顺序
    started = 1;
  } else {
    while(started == 0)
      ;
    __sync_synchronize();
    printf("hart %d starting\n", cpuid());
    kvminithart();    // turn on paging
    trapinithart();   // install kernel trap vector
    plicinithart();   // ask PLIC for device interrupts
  }

  scheduler();        
}
