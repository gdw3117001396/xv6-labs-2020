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
#define NBUTKET 13
#define HASH(id) (id % NBUTKET)
struct {
  struct buf buf[NBUF];

  struct buf head[NBUTKET];
  struct spinlock lock[NBUTKET];
} bcache;


void
binit(void)
{
  struct buf *b;


  for (int i = 0; i < NBUTKET; ++i) {
    char name[10];
    snprintf(name, sizeof(name), "bcache_%d", i);
    initlock(&bcache.lock[i], name);
    bcache.head[i].prev = &bcache.head[i];
    bcache.head[i].next = &bcache.head[i];
  }
  // Create linked list of buffers
  // 先将所有的buff挂载到0号哈希桶中，这其实不太合理
  for(b = bcache.buf; b < bcache.buf+NBUF; b++){
    b->next = bcache.head[0].next;
    b->prev = &bcache.head[0];
    initsleeplock(&b->lock, "buffer");
    bcache.head[0].next->prev = b;
    bcache.head[0].next = b;
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;
  uint bid = HASH(blockno);
  acquire(&bcache.lock[bid]);

  // Is the block already cached? 在cache中找到了buf，获取睡眠锁
  for(b = bcache.head[bid].next; b != &bcache.head[bid]; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      acquire(&tickslock);
      b->timestamp = ticks;
      release(&tickslock);

      release(&bcache.lock[bid]);
      acquiresleep(&b->lock);
      return b;
    }
  }

  // Not cached. 
  b = 0;
  struct buf *tmp;
  // 此时还拿着bcache.lock[bid]的
  for (int i = bid, cycle = 0; cycle != NBUTKET; i = (i + 1) % NBUTKET) {
    ++cycle;
    if (i != bid) {
      // bcache.lock[i]锁没人拿的时候，才可以去拿，不然两个进程都持有着各自的锁，然后都去申请对方的锁就会死锁了。
      if (!holding(&bcache.lock[i])) {
        acquire(&bcache.lock[i]);
      } else {
        continue;
      }
    }

    for (tmp = bcache.head[i].next; tmp != &bcache.head[i]; tmp = tmp->next) {
      if (tmp->refcnt == 0 && (b == 0 || b->timestamp > tmp->timestamp)) {
        b = tmp;
      }
    }

    // 从别人bucket拿来的buf要插入到自己的bucket的头部
    if (b) {
      if (i != bid) {
          b->next->prev = b->prev;
          b->prev->next = b->next;
          release(&bcache.lock[i]);
          b->next = bcache.head[bid].next;
          b->prev = &bcache.head[bid];
          bcache.head[bid].next->prev = b;
          bcache.head[bid].next = b;
      }

      acquire(&tickslock);
      b->timestamp = ticks;
      release(&tickslock);
      b->dev = dev;
      b->blockno = blockno;
      b->valid = 0;
      b->refcnt = 1;
      release(&bcache.lock[bid]);
      acquiresleep(&b->lock);
      return b;
    } else {
      if (i != bid) {
        release(&bcache.lock[i]);
      }
    }
  }
  /*for(b = bcache.head.prev; b != &bcache.head; b = b->prev){
    if(b->refcnt == 0) {
      b->dev = dev;
      b->blockno = blockno;
      b->valid = 0;
      b->refcnt = 1;
      release(&bcache.lock);
      acquiresleep(&b->lock);
      return b;
    }
  }*/
  panic("bget: no buffers");
}

// Return a locked buf with the contents of the indicated block.
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if(!b->valid) {
    virtio_disk_rw(b, 0); // 0表示读入
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked. 将修改后的缓冲区写到磁盘的相应块上，必须持有锁
void
bwrite(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1); // 1表示写入
}

// Release a locked buffer.
// Move to the head of the most-recently-used list. 
void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");

  uint bid = HASH(b->blockno);
  releasesleep(&b->lock);
  acquire(&bcache.lock[bid]);
  b->refcnt--;
  /*if (b->refcnt == 0) {
    // no one is waiting for it.
    b->next->prev = b->prev;
    b->prev->next = b->next;
    b->next = bcache.head.next;
    b->prev = &bcache.head;
    bcache.head.next->prev = b;
    bcache.head.next = b;
  }*/
  acquire(&tickslock);
  b->timestamp = ticks;
  release(&tickslock);

  release(&bcache.lock[bid]);
}

void
bpin(struct buf *b) {
  uint bid = HASH(b->blockno);
  acquire(&bcache.lock[bid]);
  b->refcnt++;
  release(&bcache.lock[bid]);
}

void
bunpin(struct buf *b) {
  uint bid = HASH(b->blockno);
  acquire(&bcache.lock[bid]);
  b->refcnt--;
  release(&bcache.lock[bid]);
}


