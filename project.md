# COL331/633 Project: Copy-On-Write

During the fork system call, xv6 creates a new page directory and copies the memory contents of the parent process into the child process. If the process *P1* allocates an array of 1MB and subsequently invokes the fork system call, xv6 assigns an additional 1MB of memory to the child process *P2* while copying the contents of *P1* into *P2*. If neither *P1* nor *P2* modify the 1MB array, then we unnecessarily created a copy. In order to improve memory utilization, we would like to extend xv6 with the copy-on-write (COW) mechanism.

The COW mechanism is a resource management technique that enables the parent and child processes to initially have access to the same memory pages without copying them. When a process, whether it is the parent or child, performs a write operation on the shared page, the page is copied and then altered.

## Design
In this section, we will systematically implement the COW mechanism along with demand paging. For simplicity, we assume that only the same virtual page number of *P1* and *P2* may point to the same physical page number. Consider an example where we create a parent process, referred to as *P1*, and subsequently create its child process, denoted as *P2* using the fork system call.

### 1. Enable Memory Page Sharing
Let's assume that the system has deactivated the swap space. In this part, process *P1* saturates the physical memory by allocating many pages, and then proceeds to execute the fork system call. xv6's fork implementation will fail because the OS will not be able to allocate same amount of memory for *P2* due to insufficient space. We should first address this issue by duplicating the page table entries (PTEs) rather than the memory pages within the `copyuvm` function in `vm.c`. Enabling the sharing of memory pages can result in one process overwriting the contents written by another process. In order to prevent this, we designate the memory page as read-only.

### 2. Handle Writes on Shared Pages
Once *P1* and *P2* have been successfully created, we now need to manage the write operations on the shared memory pages. If a process attempts to write to a shared page that was previously marked as read-only, x86 will generate the `T_PGFLT` trap signal (refer `trap.c`). Upon receiving this signal, the page fault handler shall create a new page with the same contents as the shared page. Subsequently, the page table entry of the process shall be modified to point to the new page.

#### 2.1. Optimization
In the above implementation if P1 and P2 both write to a shared page, we will end up creating three copies of that page in the memory. To reduce one copy, we shall now implement a reverse map data structure called `rmap` that tracks the number of processes that are currently referencing a memory page (*PAGE_ID* &rarr; *#PROCS*). During create or fork operations, we shall increment the value of *rmap*. Likewise, when encountering exit or kill operations, we shall decrement the value of *rmap*. In addition to these activities, during the invocation of the page fault handler, we update the values of *rmap* corresponding to the shared page and the new page accordingly. If we notice that a shared page is accessed by only one process during a page fault, we can directly mark the page as writeable instead of creating a new copy of it.  

### 3. COW with Swapping
Finally, we shall activate the swap space. If a memory page is not available, we may need to remove a shared memory page. With the implementation of Copy-on-Write (COW), it is necessary to update the page table entry (PTE) of not just the victim process, but also any other processes that are accessing the same page. The existing *rmap* just contains the reference count, which is insufficient for this operation. Thus, we extend the *rmap* data structure to retain information about the processes that are associated with the physical page. Now, we can modify the page table entries (PTEs) of the processes that are utilizing the same page. Note that in our implementation, a physical page reverse maps to same virtual page numbers in different processes. 

Additionally, it will be necessary to save the current *rmap* data corresponding to the physical page that is in the swap space. This will ensure when we bring the memory page back from disk into memory at a later time, we must update all the PTEs properly. Note: It is important to update both the swap slot and rmap data structures.

 ## Deliverables
In this assignment, you need to use the given codebase as the base code. The following additions are made on the xv6-public code:
1. We have added the following system calls to evaluate your submission: get_rss, and getNumFreePages. You are prohibited from making any changes to them.
2. We have added a configuration parameter `SWAPBLOCKS` that denotes the number of blocks dedicated to the swap space. Note that the evaluation script will override memlayout.h, and param.h.

In the project root directory, run the following commands:
```
make clean
tar czvf project_<entryNumber1>_<entryNumber2>_<entryNumber3>.tar.gz *
```

This will create a tarball with the name, project_<entryNumber1>_<entryNumber2>_<entryNumber3>.tar.gz in the same directory. Submit this tarball on Moodle. Entry number format: 2020CS10567. (All English letters will be in capitals in the entry number)

## Grading Rules
To Be Announced

## Auto-grader
To Be Announced