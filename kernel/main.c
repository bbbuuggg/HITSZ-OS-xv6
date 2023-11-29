#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "defs.h"

volatile static int started = 0;

// start() jumps here in supervisor mode on all CPUs.
void main() {
  if (cpuid() == 0) {
    consoleinit();
#if defined(LAB_PGTBL) || defined(LAB_LOCK)
    statsinit();
#endif
    printfinit();
    printf("\n");
    printf("xv6 kernel is booting\n");
    printf("\n");
    kinit();             // 物理页分配器
    kvminit();           // 创建内核页表
    kvminithart();       // 启用分页
    procinit();          // 进程表初始化
    trapinit();          // 中断向量表初始化
    trapinithart();      // 安装内核中断向量
    plicinit();          // 初始化中断控制器
    plicinithart();      // 向PLIC请求设备中断
    binit();             // 缓冲区缓存初始化
    iinit();             // inode缓存初始化
    fileinit();          // 文件表初始化
    virtio_disk_init();  // 模拟硬盘初始化
#ifdef LAB_NET
    pci_init();
    sockinit();
#endif
    userinit();  // first user process
    __sync_synchronize();
    started = 1;
  } else {
    while (started == 0)
      ;
    __sync_synchronize();
    printf("hart %d starting\n", cpuid());
    kvminithart();   // turn on paging
    trapinithart();  // install kernel trap vector
    plicinithart();  // ask PLIC for device interrupts
  }

  scheduler();
}
