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

#define NBUCKET 13
#define MAXTIME ((1u << 31) - 1 + (1u << 31))

struct Bucket {
  struct spinlock lock;
  struct buf head;
};

struct {
  struct spinlock lock;
  struct buf buf[NBUF];

  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  // struct buf head;
  struct Bucket bucket[NBUCKET];
} bcache;


void
binit(void)
{
  struct buf *b;
  int i;

  for (i = 0; i < NBUCKET; i ++ ) {
    bcache.bucket[i].head.next = 0;
    initlock(&bcache.bucket[i].lock, "bucket");
  }
  initlock(&bcache.lock, "bcache");

  // Create linked list of buffers
  // bcache.head.prev = &bcache.head;
  // bcache.head.next = &bcache.head;
  // for(b = bcache.buf; b < bcache.buf+NBUF; b++){
  //   b->next = bcache.head.next;
  //   b->prev = &bcache.head;
  //   initsleeplock(&b->lock, "buffer");
  //   bcache.head.next->prev = b;
  //   bcache.head.next = b;
  // }

  // Initialize all buckets
  for (i = 0; i < NBUF; i ++ ) {
    int id = i % NBUCKET;
    b = bcache.buf + i;
    initsleeplock(&b->lock, "buffer");

    b->next = bcache.bucket[id].head.next;
    bcache.bucket[id].head.next = b;
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;
  int id = blockno % NBUCKET;

  acquire(&bcache.bucket[id].lock);

  // Is the block already cached?
  for(b = bcache.bucket[id].head.next; b; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcache.bucket[id].lock);
      acquiresleep(&b->lock);
      return b;
    }
  }

  release(&bcache.bucket[id].lock);


  // Not cached.
  // Recycle the least recently used (LRU) unused buffer.
  // for(b = bcache.head.prev; b != &bcache.head; b = b->prev){
  //   if(b->refcnt == 0) {
  //     b->dev = dev;
  //     b->blockno = blockno;
  //     b->valid = 0;
  //     b->refcnt = 1;
  //     release(&bcache.lock);
  //     acquiresleep(&b->lock);
  //     return b;
  //   }
  // }

  // Not cached.
  // Find the least recently used unused buffer in own bucket.
  acquire(&bcache.lock);
  acquire(&bcache.bucket[id].lock);
  
  uint min_time = MAXTIME;
  struct buf* lru_b = 0;

  for(b = bcache.bucket[id].head.next; b; b = b->next){
    if(b->refcnt == 0 && b->timestamp < min_time) {
      min_time = b->timestamp;
      lru_b = b;
    }
  }
  // has found the lru unused buffer in own bucket.
  if (lru_b) {
    lru_b->dev = dev;
    lru_b->blockno = blockno;
    lru_b->valid = 0;
    lru_b->refcnt = 1;
    release(&bcache.bucket[id].lock);
    release(&bcache.lock);
    acquiresleep(&lru_b->lock);
    return lru_b;
  }
  // hasn't found the lru unused buffer in own bucket.
  // find the lru unused buffer in other buckets.
  int i;
  min_time = MAXTIME;
  int new_id = -1;
  for (i = 0; i < NBUCKET; i ++ ) {
    if (i == id) continue;
    acquire(&bcache.bucket[i].lock);
    for(b = bcache.bucket[i].head.next; b; b = b->next){
      if(b->refcnt == 0 && b->timestamp < min_time) {
        min_time = b->timestamp;
        lru_b = b;
        new_id = i;
      }
    }
    release(&bcache.bucket[i].lock);
  }

  if (lru_b == 0)
    panic("bget: no buffers");

  acquire(&bcache.bucket[new_id].lock);
  lru_b->dev = dev;
  lru_b->blockno = blockno;
  lru_b->valid = 0;
  lru_b->refcnt = 1; 
  for (b = &bcache.bucket[new_id].head; b->next; b = b->next) {
    if (b->next == lru_b) {
      b->next = lru_b->next;
      break;
    }
  }
  release(&bcache.bucket[new_id].lock);


  lru_b->next = bcache.bucket[id].head.next;
  bcache.bucket[id].head.next = lru_b;
  release(&bcache.bucket[id].lock);
  release(&bcache.lock);

  acquiresleep(&lru_b->lock);
  return lru_b;
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
  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);

  int id = b->blockno % NBUCKET;
  acquire(&bcache.bucket[id].lock);
  b->refcnt--;
  if (b->refcnt == 0) {
    // no one is waiting for it.
    acquire(&tickslock);
    b->timestamp = ticks;
    release(&tickslock);
  }
  
  release(&bcache.bucket[id].lock);
}

void
bpin(struct buf *b) {
  int id = b->blockno % NBUCKET;
  acquire(&bcache.bucket[id].lock);
  b->refcnt++;
  release(&bcache.bucket[id].lock);
}

void
bunpin(struct buf *b) {
  int id = b->blockno % NBUCKET;
  acquire(&bcache.bucket[id].lock);
  b->refcnt--;
  release(&bcache.bucket[id].lock);
}


