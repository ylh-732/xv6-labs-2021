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

struct {
  struct spinlock lock;
  struct run *freelist;
  char lock_name[9];
} kmems[NCPU];

void
kinit()
{
  for (int i = 0; i < NCPU; i++) {
    snprintf(kmems[i].lock_name, 9, "kmem_%d", i);
    initlock(&kmems[i].lock, kmems[i].lock_name);
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

  push_off();
  int id = cpuid();
  
  acquire(&kmems[id].lock);
  r->next = kmems[id].freelist;
  kmems[id].freelist = r;
  release(&kmems[id].lock);

  pop_off();
}

void *
steal(int thief_id)
{
  struct run *temp;

  for (int victim_id = 0; victim_id < NCPU; victim_id++) {
    if (victim_id == thief_id) {
      continue;
    }

    acquire(&kmems[victim_id].lock);
    if (kmems[victim_id].freelist) {
      temp = kmems[victim_id].freelist;
      kmems[victim_id].freelist = kmems[victim_id].freelist->next;
      temp->next = kmems[thief_id].freelist;
      kmems[thief_id].freelist = temp;
    }
    release(&kmems[victim_id].lock);
  }

  return (void*)kmems[thief_id].freelist;
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  push_off();
  int id = cpuid();

  acquire(&kmems[id].lock);
  r = kmems[id].freelist;
  if(r == 0)
    r = steal(id);
  if(r)
    kmems[id].freelist = r->next; 
  release(&kmems[id].lock);

  pop_off();

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}
