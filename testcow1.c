#include "types.h"
#include "stat.h"
#include "user.h"

void
test(void)
{
    int pid;
    uint prev_free_pages = getNumFreePages();
    // uint curr_free_pages = prev_free_pages;

    long long size = ((prev_free_pages - 20) * 4096); // 20 pages will be used by kernel to create kstack, and process related datastructures.

    printf(1, "Allocating %d bytes for each process\n", size);

    char *m1 = (char*)malloc(size);
    if (m1 == 0) goto out_of_memory;
    
    for(int k=0;k<size;++k){
        m1[k] = (char)(65+(k%26));
    }

    printf(1, "\n*** Forking ***\n");
    pid = fork();

    if (pid < 0) goto fork_failed; // Fork failed
    if (pid == 0) { // Child process
        printf(1, "\n*** Child ***\n");
        for(int k=0;k<size;++k){
			if(m1[k] != (char)(65+(k%26))) goto failed;
		}
        printf(1, "[COW] Lab5 Child test passed!\n");
    } else { // Parent process
        printf(1, "\n*** Parent ***\n");
        for(int k=0;k<size;++k){
            if(m1[k] != (char)(65+(k%26))) goto failed;
        }
        wait();
        // Do some processing
        int x = 0;
        for(int k=0;k<100000;++k){
            for (int j=0;j<10000;++j){
                x = j + k;
            }
        }
        printf(1, "done processing %d\n", x);
        printf(1, "[COW] Lab5 Parent test passed!\n");
    }
    exit();

failed:
    printf(1, "Copy failed: The memory contents of the processes is inconsistent!\n");
	printf(1, "Lab5 test failed!\n");
	exit();
out_of_memory:
	printf(1, "Exceeded the PHYSTOP!\n");
    printf(1, "Lab5 test failed!\n");
	exit();
fork_failed:
	printf(1, "Failed to fork a process!\n");
    printf(1, "Lab5 test failed!\n");
	exit();
}

int
main(int argc, char *argv[])
{
	printf(1, "Test starting...\n");
	test();
	return 0;
}
