// Physical memory allocator, intended to allocate
// memory for user processes, kernel stacks, page table pages,
// and pipe buffers. Allocates 4096-byte pages.

#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "spinlock.h"
#include "pageswap.c"

void freerange(void *vstart, void *vend);
extern char end[]; // first address after kernel loaded from ELF file
                   // defined by the kernel linker script in kernel.ld

struct run
{
    struct run *next;
};

struct
{
    struct spinlock lock;
    int use_lock;
    uint num_free_pages; // store number of free pages
    struct run *freelist;
} kmem;

struct
{
    int use_lock;
    struct spinlock lock;
    int ref[PHYSTOP / PGSIZE];
    ull pid[PHYSTOP / PGSIZE];
} rmap;

void lock_rmap()
{
    if (rmap.use_lock)
        acquire(&rmap.lock);

    return;
}
void release_rmap()
{
    if (rmap.use_lock)
        release(&rmap.lock);
    return;
}

ull getRmapPagePid(uint pa) {
    lock_rmap();

    ull result_pids = rmap.pid[pa / PGSIZE];

    release_rmap();

    return result_pids;
}

void setRmapPagePid(uint pa, uint pid) {
    lock_rmap();

    rmap.pid[pa / PGSIZE] |= (1ULL<<pid);

    release_rmap();
}

void unsetRmapPagePid(uint pa, uint pid) {
    lock_rmap();

    rmap.pid[pa / PGSIZE] &= ~(1ULL<<pid);

    release_rmap();
}

void setAllRmapPagePid(uint pa, ull val) {
    lock_rmap();

    rmap.pid[pa / PGSIZE] = val;

    release_rmap();
}

int getRmapRef(uint pa)
{
    lock_rmap();

    int num = rmap.ref[pa / PGSIZE];
    
    release_rmap();
    
    return num;
}

void setRmapRef(uint pa, int val)
{
    lock_rmap();
    
    rmap.ref[pa / PGSIZE] = val;
    
    release_rmap();
}

void incRmapRef(uint pa)
{
    lock_rmap();

    ++rmap.ref[pa / PGSIZE];
    
    release_rmap();
}

void decRmapRef(uint pa)
{
    lock_rmap();

    --rmap.ref[pa / PGSIZE];
    
    release_rmap();
}

// Initialization happens in two phases.
// 1. main() calls kinit1() while still using entrypgdir to place just
// the pages mapped by entrypgdir on free list.
// 2. main() calls kinit2() with the rest of the physical pages
// after installing a full page table that maps them on all cores.
void kinit1(void *vstart, void *vend)
{
    initlock(&kmem.lock, "kmem");
    initlock(&rmap.lock, "rmap");
    rmap.use_lock = 0;
    kmem.use_lock = 0;
    freerange(vstart, vend);
}

void kinit2(void *vstart, void *vend)
{
    freerange(vstart, vend);
    rmap.use_lock = 1;
    kmem.use_lock = 1;
}

void freerange(void *vstart, void *vend)
{
    char *p;
    p = (char *)PGROUNDUP((uint)vstart);
    for (; p + PGSIZE <= (char *)vend; p += PGSIZE)
    {
        rmap.ref[V2P(p) / PGSIZE] = 0;
        rmap.pid[V2P(p) / PGSIZE] = 0ULL;
        kfree(p);
        //   kmem.num_free_pages += 1;
    }
}
// PAGEBREAK: 21
//  Free the page of physical memory pointed at by v,
//  which normally should have been returned by a
//  call to kalloc().  (The exception is when
//  initializing the allocator; see kinit above.)
void kfree(char *v)
{
    struct run *r;

    if ((uint)v % PGSIZE || v < end || V2P(v) >= PHYSTOP)
    {
        cprintf("page aligned : %d\n", (uint)v % PGSIZE);
        cprintf("virtual end : %d\n", v < end);
        cprintf("larger then physical mem : %d\n", V2P(v) >= PHYSTOP);
        panic("kfree");
    }

    // If the number of references is greater than 1,
    // there is no need to free the memory
    decRmapRef(V2P(v));
    if (getRmapRef(V2P(v)) > 0)
    {
        return;
    }

    // Fill with junk to catch dangling refs.
    memset(v, 1, PGSIZE);
    setAllRmapPagePid(V2P(v), 0ULL);
    if (kmem.use_lock)
        acquire(&kmem.lock);
    r = (struct run *)v;
    r->next = kmem.freelist;
    kmem.num_free_pages += 1;
    kmem.freelist = r;
    if (kmem.use_lock)
        release(&kmem.lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
char *
kalloc(void)
{
    struct run *r;

    if (kmem.use_lock)
        acquire(&kmem.lock);
    r = kmem.freelist;
    if (r)
    {
        kmem.freelist = r->next;
        kmem.num_free_pages -= 1;
        setRmapRef(V2P(r), 1);
    }

    if (kmem.use_lock)
        release(&kmem.lock);

    if (SWAPON)
    {
        if (!r)
        {
            swapOut();
            return kalloc();
        }
    }

    return (char *)r;
}

uint num_of_FreePages(void)
{
    acquire(&kmem.lock);

    uint num_free_pages = kmem.num_free_pages;

    release(&kmem.lock);

    return num_free_pages;
}
