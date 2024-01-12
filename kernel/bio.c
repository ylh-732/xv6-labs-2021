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
#define NBUFPBUCKET ((uint)(NBUF/NBUCKET))

struct {
  struct spinlock global_lock;
  struct buf buf[NBUF];

  struct spinlock bucket_lock[NBUCKET];
  struct buf* bucket_head[NBUCKET];
} bcache;

void 
add_first(int bucket, struct buf* b)
{
  struct buf* head = bcache.bucket_head[bucket];
  b->next = head;
  bcache.bucket_head[bucket] = b;
}

void
binit(void)
{
  struct buf *b;

  initlock(&bcache.global_lock, "global");

  for (int i = 0; i < NBUCKET; i++) {
    char lock_name[9];
    snprintf(lock_name, 9, "bcache_%d", i);
    initlock(&bcache.bucket_lock[i], lock_name);
  }
  
  for (b = bcache.buf; b < bcache.buf + NBUF; b++) {
    initsleeplock(&b->lock, "buffer");
    add_first(0, b);
  }
}

int
evict(int bucket, struct buf* target)
{
  struct buf* prev = bcache.bucket_head[bucket];
  struct buf* b = bcache.bucket_head[bucket]->next;

  if (prev->blockno == target->blockno) {
    bcache.bucket_head[bucket] = bcache.bucket_head[bucket]->next;
    return 0;
  }

  while (b->blockno != target->blockno) {
    prev = b;
    b = b->next;
  }
  if (b->blockno != target->blockno) {
    return -1;
  }

  prev->next = b->next;
  return 0;
}

struct buf*
lru_unused_buf(int bucket)
{
  struct buf *b;
  struct buf *lru_b = 0;
  uint min_ticks = 0xffffffff;

  for (b = bcache.bucket_head[bucket]; b != 0; b = b->next) {
    if (b->refcnt == 0 && b->time_stamp < min_ticks) {
      min_ticks = b->time_stamp;
      lru_b = b;
    }
  }

  return lru_b;
}

void
update_buf(struct buf* b, uint dev, uint blockno)
{
  b->dev = dev;
  b->blockno = blockno;
  b->valid = 0;
  b->refcnt = 1;
  b->time_stamp = ticks;
}

void
invariant()
{
  struct buf* b;
  for (int bucket = 0; bucket < NBUCKET; bucket++) {
    acquire(&bcache.bucket_lock[bucket]);
    printf("bucket: %d\n", bucket);
    for (b = bcache.bucket_head[bucket]; b != 0; b = b->next) {
      printf("blockno: %d, refcnt: %d, time_stamp: %d\n", b->blockno, b->refcnt, b->time_stamp);
    }
    release(&bcache.bucket_lock[bucket]);
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;
  int bucket = blockno % NBUCKET;

  // Is the block already cached?
  acquire(&bcache.bucket_lock[bucket]);
  for(b = bcache.bucket_head[bucket]; b != 0; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      b->time_stamp = ticks;
      release(&bcache.bucket_lock[bucket]);
      acquiresleep(&b->lock);
      return b;
    }
  }
  release(&bcache.bucket_lock[bucket]);

  struct buf *lru_b = 0;
  acquire(&bcache.global_lock);

  acquire(&bcache.bucket_lock[bucket]);
  for(b = bcache.bucket_head[bucket]; b != 0; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      b->time_stamp = ticks;
      release(&bcache.bucket_lock[bucket]);
      release(&bcache.global_lock);
      acquiresleep(&b->lock);
      return b;
    }
  }
  release(&bcache.bucket_lock[bucket]);

  for (int i = bucket; i < bucket + NBUCKET; i++) {
    int idx = i % NBUCKET;

    acquire(&bcache.bucket_lock[idx]);
    lru_b = lru_unused_buf(idx);
    if (lru_b != 0) {
      if (evict(idx, lru_b) < 0) {
        panic("evict");
      }
      release(&bcache.bucket_lock[idx]);

      acquire(&bcache.bucket_lock[bucket]);
      add_first(bucket, lru_b);
      update_buf(lru_b, dev, blockno);
      release(&bcache.bucket_lock[bucket]);
      release(&bcache.global_lock);

      acquiresleep(&lru_b->lock);
      return lru_b;
    }
    release(&bcache.bucket_lock[idx]);
  }
 
  release(&bcache.global_lock);

  invariant();
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
  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);

  int bucket = b->blockno % NBUCKET;
  acquire(&bcache.bucket_lock[bucket]);
  b->refcnt--;
  if (b->refcnt == 0) {
    b->time_stamp = ticks;
  }
  release(&bcache.bucket_lock[bucket]);
}

void
bpin(struct buf *b) {
  int bucket = b->blockno % NBUCKET;

  acquire(&bcache.bucket_lock[bucket]);
  b->refcnt++;
  release(&bcache.bucket_lock[bucket]);
}

void
bunpin(struct buf *b) {
  int bucket = b->blockno % NBUCKET;

  acquire(&bcache.bucket_lock[bucket]);
  b->refcnt--;
  release(&bcache.bucket_lock[bucket]);
}


