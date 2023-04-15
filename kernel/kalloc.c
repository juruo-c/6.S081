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
} kmem[NCPU];

int 
getcpuid() {
  push_off();
  int id = cpuid();
  pop_off();
  return id;
}

void
kinit()
{
  int id;
  for (id = 0; id < NCPU; id ++ )
    initlock(&kmem[id].lock, "kmem");
  
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);
  printf("out freerange");
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

  int cpuid = getcpuid();


  acquire(&kmem[cpuid].lock);
  r->next = kmem[cpuid].freelist;
  kmem[cpuid].freelist = r;
  release(&kmem[cpuid].lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;
  int cpuid = getcpuid();
  int id;

  acquire(&kmem[cpuid].lock);
  r = kmem[cpuid].freelist;
  if(r)
    kmem[cpuid].freelist = r->next;
  release(&kmem[cpuid].lock);

  if(!r) {
    for (id = 0; id < NCPU; id ++ ) {
      if (id != cpuid) {
        acquire(&kmem[id].lock);
        r = kmem[id].freelist;
        if(r)
          kmem[id].freelist = r->next;
        release(&kmem[id].lock);
        if(r) break;
      }
    }
  }

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}
