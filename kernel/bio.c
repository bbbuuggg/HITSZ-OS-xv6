// Buffer cache.
//
// The buffer cache is a linked list of buf structures holding
// cached copies of disk block contents.  Caching disk blocks
// in memory reduces the number of disk reads and also provides
// a synchronization point for disk blocks used by multiple processes.
//
// Interface:
// * To get a buffer for a particular disk block, call bread.
// * After changing buffer data, call bwrite to write it to disk.
// * When done with the buffer, call brelse.
// * Do not use the buffer after calling brelse.
// * Only one process at a time can use a buffer,
//     so do not keep them longer than necessary.


#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"
//哈希桶的方法
int
hash(int blockno){
  return blockno % 13;//看了下貌似NBUF是30,但是我用15又跑不通、、、
}
struct {
  struct spinlock lock[13];//个锁
  struct buf buf[NBUF];

  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  // struct buf head;
  struct buf hasht[13];//13个桶
} bcache;

void
binit(void)
{
  struct buf *b;
  for(int i=0;i<13;i++){//13个桶13个锁
    initlock(&bcache.lock[i],"bcache");
    // Create linked list of buffers
    bcache.hasht[i].prev = &bcache.hasht[i];
    bcache.hasht[i].next = &bcache.hasht[i];
  }
  int i = 0;
  //添加链表
  for(b = bcache.buf; b < bcache.buf+NBUF; b++){
    b->next = bcache.hasht[i*13/NBUF].next;//这里想平均分到桶里，他这里应该还是用的NBUF计数，那b/（NBUF/13）应该可以表示当前塞得桶
    b->prev = &bcache.hasht[i*13/NBUF];
    initsleeplock(&b->lock, "buffer");
    bcache.hasht[i*13/NBUF].next->prev = b;
    bcache.hasht[i*13/NBUF].next = b;
    i++;
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  //获取桶号
  int num = hash(blockno);
  struct buf *b;
  int bingo = 0;//命中标记位

  acquire(&bcache.lock[num]);

  // Is the block already cached?
  for(b = bcache.hasht[num].next; b != &bcache.hasht[num]; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcache.lock[num]);
      acquiresleep(&b->lock);
      return b;
    }
  }

  // Not cached.
  // Recycle the least recently used (LRU) unused buffer.
  for(b = bcache.hasht[num].prev; b != &bcache.hasht[num]; b = b->prev){
    if(b->refcnt == 0) {
      b->dev = dev;
      b->blockno = blockno;
      b->valid = 0;
      b->refcnt = 1;
      bingo = 1;
      release(&bcache.lock[num]);
      acquiresleep(&b->lock);
      return b;
    }
  }
  
  if (!bingo) {
    for (int i = 0; ; i = (i + 1) % 13) {//13个桶
      if(i == num)
        continue;
      acquire(&bcache.lock[i]);
      for(b = bcache.hasht[i].prev; b != &bcache.hasht[i]; b = b->prev){
        if(b->refcnt == 0) {
        //   b->dev = dev;
        //   b->blockno = blockno;
        //   b->valid = 0;
        //   b->refcnt = 1;
        //   bingo = 1;
        //   release(&bcache.lock[i]);
        //   acquiresleep(&b->lock);
        //   release(&bcache.lock[num]);
        //   return b;
        //淦！不能这么写，for循环里面return了之后总会有锁没有release，代码量一大全是炸弹、、、
        //取出来
          b->prev->next = b->next;
          b->next->prev = b->prev;
          bingo = 1;
          release(&bcache.lock[i]);
          break;
        }
      }
      if(!bingo){
        release(&bcache.lock[i]);//接着找
      }else{
        break;
      }
    }
  }
  if(bingo){
    b->next = bcache.hasht[num].next;
    b->prev = &bcache.hasht[num];
    bcache.hasht[num].next->prev = b;
    bcache.hasht[num].next = b;
    b->dev = dev;
    b->blockno = blockno;
    b->valid = 0;
    b->refcnt = 1;
    release(&bcache.lock[num]);
    acquiresleep(&b->lock);
    return b;
  }
  panic("bget: no buffers");
}


// Return a locked buf with the contents of the indicated block.
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if(!b->valid) {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void
bwrite(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void
brelse(struct buf *b)
{
  int num = hash(b->blockno);
  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);

  acquire(&bcache.lock[num]);
  b->refcnt--;
  if (b->refcnt == 0) {
    // no one is waiting for it.
    b->next->prev = b->prev;
    b->prev->next = b->next;
    b->next = bcache.hasht[num].next;
    b->prev = &bcache.hasht[num];
    bcache.hasht[num].next->prev = b;
    bcache.hasht[num].next = b;
  }
  
  release(&bcache.lock[num]);
}

void
bpin(struct buf *b) {
  int num = hash(b->blockno);
  acquire(&bcache.lock[num]);
  b->refcnt++;
  release(&bcache.lock[num]);
}

void
bunpin(struct buf *b) {
  int num = hash(b->blockno);
  acquire(&bcache.lock[num]);
  b->refcnt--;
  release(&bcache.lock[num]);
}
