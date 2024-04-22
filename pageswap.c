#include "types.h"
#include "defs.h"
#include "param.h"
#include "stat.h"
// #include "mmu.h"
// #include "spinlock.h"
#include "sleeplock.h"
#include "proc.h"
#include "fs.h"
#include "buf.h"
#include "file.h"
#include "swap.h"
#include "x86.h"
#define SWAPPAGES SWAPBLOCKS / 8
struct swapinfo
{
    // int pid;
    int refCount;
    ull pidRef;
    int page_perm;
    int is_free;
};

struct swapinfo swp[SWAPPAGES];

  // buffer 

void swapInit(void) {
    for (int i = 0; i < SWAPPAGES; i++) {
        swp[i].page_perm = 0;
        swp[i].is_free = 1;
        swp[i].refCount = 0;
        swp[i].pidRef = 0ULL;
    }
}

void swapOut()
{
    struct superblock sb;
    readsb(ROOTDEV, &sb);
    // find victim page
    struct proc *p = find_victim_process();
    uint victim_page_VA = find_victim_page(p);
    pte_t *victim_pte = walkpgdir(p->pgdir, (void*)victim_page_VA, 0);

    int i;
    // find free swap slot
    for (i = 0; i < SWAPPAGES; ++i)
    {
        if (swp[i].is_free) {
            break;
        }
    }
    // cprintf("[SWAPOUT] Current slot : %d\n", i);
    swp[i].page_perm = 0;
    swp[i].is_free = 0;
    // swp[i].pid = p->pid;
    swp[i].refCount = getRmapRef(PTE_ADDR(*victim_pte));
    swp[i].pidRef = getRmapPagePid(PTE_ADDR(*victim_pte));

    // update swap slot permissions to match victim page permissions 
    swp[i].page_perm = PTE_FLAGS(*victim_pte);

    char *pa = (char*)P2V(PTE_ADDR(*victim_pte));
    uint addressOffset;
    for (int j = 0; j < 8; ++j)
    {
        addressOffset = PTE_ADDR(*victim_pte) + (j * BSIZE);
        struct buf *b = bread(ROOTDEV, sb.swapstart + (8*i) + j);
        memmove(b->data, (void *)P2V(addressOffset), BSIZE);
        b->blockno = (sb.swapstart + (8 * i) + j);
        b->flags |= B_DIRTY;
        b->dev = ROOTDEV;
        bwrite(b);
        brelse(b);
    }
    (*victim_pte) = ((sb.swapstart + (8 * i)) << 12) & (~0xFFF);
    *victim_pte &= ~PTE_P;
    for (int i = 0; i < 64; i++) {
        if (swp[i].pidRef & (1ULL << i)) {
            // cprintf("[PID] Process PID : %d\n uses a COW Page 0x%x\n", (i + 1), victim_page_VA);
            struct proc *p1 = getProcByPid(i + 1);
            pte_t *pte = walkpgdir(p1->pgdir, (void *)(victim_page_VA), 0);
            *pte = (*victim_pte);
            // lcr3(V2P(p1->pgdir));
        }
    }
    // lcr3(V2P(p->pgdir));
    setRmapRef(V2P(pa), 1);
    setAllRmapPagePid(V2P(pa), 0ULL);
    kfree(pa);
}

void swapIn(char *memory)
{
    struct proc *p = myproc();
    uint addr = rcr2();
    pde_t *pd = p->pgdir;
    pte_t *pg = walkpgdir(pd, (void *)(addr), 0);

    uint swapSlot = (*pg >> 12);    // physical address of swap block
    int swapIdx = (swapSlot - 2) / 8;

    *pg = (V2P((uint)memory)&(~0xFFF)) | swp[swapIdx].page_perm | PTE_P;
    for(int i = 0; i < 8; i++)      // writes page into physical memory
    {
        struct buf *b = bread(ROOTDEV, swapSlot+i);
        memmove(memory, b->data, BSIZE);
        brelse(b);
        memory += BSIZE;
    }
    
    for (int i = 0; i < 64; i++)
    {
        if (swp[i].pidRef & (1ULL << i))
        {
            struct proc *p1 = getProcByPid(i + 1);
            pte_t *pte = walkpgdir(p1->pgdir, (void *)(addr), 0);
            // *pte = V2P(memory) | swp[swapIdx].page_perm | PTE_P;
            *pte = *pg;
        }
    }

    setRmapRef(PTE_ADDR(*pg), swp[swapIdx].refCount);
    setAllRmapPagePid(PTE_ADDR(*pg), swp[swapIdx].pidRef);
    swp[swapIdx].is_free = 1;
    swp[swapIdx].pidRef = 0ULL;
    swp[swapIdx].refCount = 0;
    // swp[swapIdx].pid = 0;
}

void freeSwapSlot(int pid) {
    struct superblock sb;
    readsb(ROOTDEV, &sb);
    int i=0;
    for (i = 0; i < SWAPPAGES; i++) {
        if (swp[i].pidRef & (1ULL << (pid-1))) {
            swp[i].pidRef &= ~(1ULL << (pid - 1));
            --swp[i].refCount;
            
            if (swp[i].refCount == 0 && swp[i].is_free == 0) {
                for (int j = 0; j < 8; j++) {
                    struct buf *b = bread(ROOTDEV, sb.swapstart + (8 * i) + j);
                    memset(b->data, 0, BSIZE);
                    brelse(b);
                }
                swp[i].is_free = 1;
                swp[i].pidRef = 0ULL;
                swp[i].refCount = 0;
                // swp[i].pid = 0;
            }
        }
    }
}