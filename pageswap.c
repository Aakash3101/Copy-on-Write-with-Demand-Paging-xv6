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
    int pid;
    int page_perm;
    int is_free;
};

struct swapinfo swp[SWAPPAGES];

  // buffer 

void swapInit(void) {
    for (int i = 0; i < SWAPPAGES; i++) {
        swp[i].page_perm = 0;
        swp[i].is_free = 1;
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
    swp[i].pid = p->pid;

    // update swap slot permissions to match victim page permissions 
    swp[i].page_perm = PTE_FLAGS(*victim_pte);

    // cprintf("Victim page addr : %p\n", PTE_ADDR(*victim_page_VA));
    char *pa = (char*)P2V(PTE_ADDR(*victim_pte));
    uint addressOffset;
    for (int j = 0; j < 8; ++j)
    {
        // cprintf("Victim page copied Block no : %d\n", sb.swapstart + (8 * i) + j);
        addressOffset = PTE_ADDR(*victim_pte) + (j * BSIZE);
        struct buf *b = bread(ROOTDEV, sb.swapstart + (8*i) + j);
        memmove(b->data, (void *)P2V(addressOffset), BSIZE);
        b->blockno = (sb.swapstart + (8 * i) + j);
        b->flags |= B_DIRTY;
        b->dev = ROOTDEV;
        bwrite(b);
        brelse(b);
    }
    // cprintf("[SWAPOUT] Swap slot used : %d\n", sb.swapstart + 8 * i);
    (*victim_pte) = ((sb.swapstart + (8 * i)) << 12) & (~0xFFF);
    *victim_pte &= ~PTE_P;
    lcr3(V2P(p->pgdir));
    // cprintf("[SWAPOUT] Swap slot block no : %d\n", *victim_pte >> 12);
    // cprintf("[SWAPOUT] VA : %p\n", pa);
    cprintf("[SWAPOUT] kfree\n");
    kfree(pa);
    // cprintf("[SWAPOUT] Page freed by swapOut\n");
    // return (char *)victim_page_VA;
}

void swapIn(char *memory)
{
    struct proc *p = myproc();
    uint addr = rcr2();
    // cprintf("[SWAPIN] Process is requesting page : %p\n", V2P(addr));
    pde_t *pd = p->pgdir;
    pte_t *pg = walkpgdir(pd, (void *)(addr), 0);

    uint swapSlot = (*pg >> 12);    // physical address of swap block
    // cprintf("[SWAPIN] Swap slot block no : %p\n", swapSlot);
    int swapIdx = (swapSlot - 2) / 8;
    *pg = (V2P((uint)memory)&(~0xFFF)) | swp[swapIdx].page_perm;
    // int swapSlot = swap;         // swap block number (convert it to integer)
    for(int i = 0; i < 8; i++)      // writes page into physical memory
    {
        struct buf *b = bread(ROOTDEV, swapSlot+i);
        memmove(memory, b->data, BSIZE);
        brelse(b);
        memory += BSIZE;
    }
    swp[swapIdx].is_free = 1;
    swp[swapIdx].pid = 0;

    // cprintf("[SWAPIN] Swap slot freed : %d\n", swapIdx);
    // swp[swapSlot].page_perm = 0;
}

void freeSwapSlot(int pid) {
    struct superblock sb;
    readsb(ROOTDEV, &sb);
    int i;
    for (i = 0; i < SWAPPAGES; i++) {
        if (swp[i].pid == pid)
            break;
    }
    for (int j = 0; j < 8; j++) {
        struct buf *b = bread(ROOTDEV, sb.swapstart + (8 * i) + j);
        memset(b->data, 0, BSIZE);
        brelse(b);
    }
    swp[i].is_free = 0;
    swp[i].pid = 0;
}