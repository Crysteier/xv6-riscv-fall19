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

#define N_BUCKETS 13
#define hash_func(x) ((x) % N_BUCKETS)

struct {
  struct spinlock lock;
  struct buf buf[NBUF];

  // Linked list of all buffers, through prev/next.
  // head.next is most recently used.
  struct buf head;
} bcache;

struct hash_table{
  struct spinlock lock;
  struct buf head;
};

struct hash_table h_table[N_BUCKETS];

static void
touch(struct buf* b) //umiestnit tuto funkciu vzdy, ked pracujem s bufferom -> brelse, bget, bread etc.
{
  acquire(&tickslock);
  b->timestamp = ticks;
  release(&tickslock);
}

void
binit(void)
{
  struct buf *b;

  initlock(&bcache.lock, "bcache");

  for (int i=0; i<N_BUCKETS; ++i){
    h_table[i].head.prev = &h_table[i].head;
    h_table[i].head.next = &h_table[i].head;
    initlock(&h_table[i].lock, "bcache.bucket");
  }

  // Create linked list of buffers
  for(b = bcache.buf; b < bcache.buf+NBUF; b++){
    b->next = h_table[0].head.next;
    b->prev = &h_table[0].head;
    initsleeplock(&b->lock, "buffer");
    initlock(&b->reflock,"bufreflock");
    h_table[0].head.next->prev = b;
    h_table[0].head.next = b;
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno) //pozname zariadenie, aj cislo bloku => vieme zistit cislo bucketu
{
  struct buf *b;

  int bucket_i = hash_func(blockno);  //cislo bucketu
  struct hash_table *h = h_table+bucket_i; //hashtable = bucket
  acquire(&h->lock); //uz mam zamknuty len 1 riadok, nie vsetko
  // acquire(&bcache.lock);

  // Is the block already cached?
  for(b = h->head.next; b != &h->head; b = b->next){  //prechadzam buffre v danom buckete, nie vo vsetkych
    if(b->dev == dev && b->blockno == blockno){
      acquire(&b->reflock);
      b->refcnt++;
      release(&b->reflock);
      release(&h->lock);
      acquiresleep(&b->lock);
      touch(b);
      return b;
    }
  }

  // Not cached; recycle an unused buffer.
  // musim prejst vsetkymi bufframi a najst najstarsie pouzity => musim si najst nejaku casovu peciatku, kedy som ho pouzil (buf.h)
  acquire(&bcache.lock);

  struct buf *chosen = 0;
  uint oldest_timestamp = ~0;
  for(b = bcache.buf; b != bcache.buf + NBUF; b++){
    acquire(&b->reflock);
    if(b->refcnt == 0 && b->timestamp < oldest_timestamp) {
      if (chosen) {
        release(&chosen->reflock);
      }
      chosen = b;
      oldest_timestamp = b->timestamp;
    }
    else{
      release(&b->reflock);
    }
  }
  if (chosen) {
    int old_bucket_i = hash_func(chosen->blockno);
    if (old_bucket_i != bucket_i){
      chosen->next->prev = chosen->prev;
      chosen->prev->next = chosen->next;
      chosen->next = h->head.next;
      chosen->prev = &h->head;
      h->head.next->prev = chosen;
      h->head.next = chosen;
    }
    chosen->dev = dev;
    chosen->blockno = blockno;
    chosen->valid = 0;
    chosen->refcnt = 1;
    release(&chosen->reflock);
    release(&bcache.lock);
    release(&h->lock);
    acquiresleep(&chosen->lock);
    touch(chosen);
    return chosen;
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
    virtio_disk_rw(b->dev, b, 0);
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
  virtio_disk_rw(b->dev, b, 1);
}

// Release a locked buffer.
// Move to the head of the MRU list.
void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);
  acquire(&b->reflock);
  b->refcnt--;
  touch(b);
  release(&b->reflock);
}

void
bpin(struct buf *b) {
  acquire(&b->reflock);
  b->refcnt++;
  release(&b->reflock);
}

void
bunpin(struct buf *b) {
  acquire(&b->reflock);
  b->refcnt--;
  release(&b->reflock);
}


