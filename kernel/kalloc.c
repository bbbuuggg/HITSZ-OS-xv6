// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct kmem{//按原本那么写会报错？？？？？
  struct spinlock lock;
  struct run *freelist;
};
struct kmem kmems[NCPU];//一个cpu一个链表



void
kinit()//不知道为啥那种kmem_%d用snprint的那种跑不通
{
  for(int i=0;i<NCPU;i++)
  {
    initlock(&kmems[i].lock, "kmem");
  }
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  //获取cpuid
  push_off();
  int cpu_id = cpuid();//貌似先申请变量会报错，编译器抽风吧可能
  pop_off();
  //给当前的cpu链表续上就好了
  acquire(&kmems[cpu_id].lock);
  r->next = kmems[cpu_id].freelist;
  kmems[cpu_id].freelist = r;
  release(&kmems[cpu_id].lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;
  //获取cpuid号
  push_off();
  int cpu_id = cpuid();
  pop_off();

  acquire(&kmems[cpu_id].lock);
  r = kmems[cpu_id].freelist;
  if(r){//当前就够用
    kmems[cpu_id].freelist = r->next;
    release(&kmems[cpu_id].lock);
  }else{//要窃取
    release(&kmems[cpu_id].lock);
    for(int i = 0; i<NCPU ; i++){//&& i != cpu_id加这个貌似会提前结束for循环
      if(i == cpu_id)
        continue;
      acquire(&kmems[i].lock);
      r = kmems[i].freelist;
      if(r){
        kmems[i].freelist = r->next;
        release(&kmems[i].lock);
        break;
      }
      release(&kmems[i].lock);
    }
  }

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}
