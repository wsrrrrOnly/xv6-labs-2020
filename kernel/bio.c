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

// 哈希桶数量（建议使用素数）
#define NBUCKET 13
#define HASH(blockno) ((blockno) % NBUCKET)

struct hashbuf {
  struct buf head;       // 双向链表头节点（哨兵）
  struct spinlock lock;  // 每个桶独立的自旋锁
};


struct {
  struct spinlock alloc_lock; 
  struct buf buf[NBUF];
  struct hashbuf buckets[NBUCKET];
} bcache;


void
binit(void)
{
  struct buf *b;
  char lockname[16];

  initlock(&bcache.alloc_lock, "bcache_alloc");

  // 初始化所有哈希桶
  for (int i = 0; i < NBUCKET; i++) {
    snprintf(lockname, sizeof(lockname), "bcache_%d", i);
    initlock(&bcache.buckets[i].lock, lockname);

    // 初始化双向链表：head 自环
    bcache.buckets[i].head.prev = &bcache.buckets[i].head;
    bcache.buckets[i].head.next = &bcache.buckets[i].head;
  }

  // 将所有缓冲区初始挂到 bucket[0]（任意桶均可，启动时未分配）
  for (b = bcache.buf; b < bcache.buf + NBUF; b++) {
    initsleeplock(&b->lock, "buffer");

    // 头插法插入到 bucket[0]
    b->next = bcache.buckets[0].head.next;
    b->prev = &bcache.buckets[0].head;
    bcache.buckets[0].head.next->prev = b;
    bcache.buckets[0].head.next = b;

    // 初始化时间戳（可选，设为0）
    b->timestamp = 0;
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  int bid = HASH(blockno);

  acquire(&bcache.buckets[bid].lock);

  // 1. 查找是否已缓存
  for (struct buf *b = bcache.buckets[bid].head.next;
       b != &bcache.buckets[bid].head; b = b->next) {
    if (b->dev == dev && b->blockno == blockno) {
      b->refcnt++;
      acquire(&tickslock);
      b->timestamp = ticks;
      release(&tickslock);
      release(&bcache.buckets[bid].lock);
      acquiresleep(&b->lock);
      return b;
    }
  }

  // 2. 未命中：释放桶锁，准备串行化分配
  release(&bcache.buckets[bid].lock);

  // 3. 获取全局分配锁（串行化）
  acquire(&bcache.alloc_lock);

  // 4. 重新获取桶锁
  acquire(&bcache.buckets[bid].lock);

  // 🔑 5. 二次检查：是否已被别人加载？
  for (struct buf *b = bcache.buckets[bid].head.next;
       b != &bcache.buckets[bid].head; b = b->next) {
    if (b->dev == dev && b->blockno == blockno) {
      b->refcnt++;
      acquire(&tickslock);
      b->timestamp = ticks;
      release(&tickslock);
      release(&bcache.buckets[bid].lock);
      release(&bcache.alloc_lock);
      acquiresleep(&b->lock);
      return b;
    }
  }

  // 6. 分配 victim（在 alloc_lock 保护下，安全遍历）
  struct buf *victim = 0;
  for (int i = 0; i < NBUCKET; i++) {
    for (struct buf *b = bcache.buckets[i].head.next;
         b != &bcache.buckets[i].head; b = b->next) {
      if (b->refcnt == 0) {
        if (victim == 0 || b->timestamp < victim->timestamp) {
          victim = b;
        }
      }
    }
  }

  if (victim == 0)
    panic("bget: no buffers");

  // 7. 如果 victim 不在目标桶，迁移它
  int victim_bid = HASH(victim->blockno);
  if (victim_bid != bid) {
    // 从原桶移除
    victim->next->prev = victim->prev;
    victim->prev->next = victim->next;

    // 插入到目标桶
    victim->next = bcache.buckets[bid].head.next;
    victim->prev = &bcache.buckets[bid].head;
    bcache.buckets[bid].head.next->prev = victim;
    bcache.buckets[bid].head.next = victim;
  }

  // 8. 初始化
  victim->dev = dev;
  victim->blockno = blockno;
  victim->valid = 0;
  victim->refcnt = 1;
  acquire(&tickslock);
  victim->timestamp = ticks;
  release(&tickslock);

  release(&bcache.buckets[bid].lock);
  release(&bcache.alloc_lock);
  acquiresleep(&victim->lock);
  return victim;
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
  if (!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);

  int bid = HASH(b->blockno);
  acquire(&bcache.buckets[bid].lock);
  b->refcnt--;

  acquire(&tickslock);
  b->timestamp = ticks;
  release(&tickslock);

  release(&bcache.buckets[bid].lock);
}

void
bpin(struct buf *b) {
  int bid = HASH(b->blockno);
  acquire(&bcache.buckets[bid].lock);
  b->refcnt++;
  release(&bcache.buckets[bid].lock);
}

void
bunpin(struct buf *b) {
  int bid = HASH(b->blockno);
  acquire(&bcache.buckets[bid].lock);
  b->refcnt--;
  release(&bcache.buckets[bid].lock);
}
