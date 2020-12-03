/*
 * phase3d.c
 *
 */

/***************

NOTES ON SYNCHRONIZATION

There are various shared resources that require proper synchronization. 

Swap space. Free swap space is a shared resource, we don't want multiple pagers choosing the
same free space to hold a page. You'll need a mutex around the free swap space.

The clock hand is also a shared resource.

The frames are a shared resource in that we don't want multiple pagers to choose the same frame via
the clock algorithm. That's the purpose of marking a frame as "busy" in the pseudo-code below. 
Pagers ignore busy frames when running the clock algorithm.

A process's page table is a shared resource with the pager. The process changes its page table
when it quits, and a pager changes the page table when it selects one of the process's pages
in the clock algorithm. 

Normally the pagers would perform I/O concurrently, which means they would release the mutex
while performing disk I/O. I made it simpler by having the pagers hold the mutex while they perform
disk I/O.

***************/


#include <assert.h>
#include <phase1.h>
#include <phase2.h>
#include <usloss.h>
#include <string.h>
#include <libuser.h>

#include "phase3.h"
#include "phase3Int.h"

#ifdef DEBUG
static int debugging3 = 1;
#else
static int debugging3 = 0;
#endif

static void debug3(char *fmt, ...)
{
    va_list ap;

    if (debugging3) {
        va_start(ap, fmt);
        USLOSS_VConsole(fmt, ap);
    }
}

typedef struct Pages{
    Block *block;
}Pages;

typedef struct Block{
    int track;
    int swap;
}

typedef struct Frame{
    int *pid;
    int *pages;
}Frame;

static int initialized = 0;
static SID semSwap;
static SID semVMStats;
static Pages *processes;
static int num_pages;
static Frame *frame_processes;

/*
 *----------------------------------------------------------------------
 *
 * P3SwapInit --
 *
 *  Initializes the swap data structures.
 *
 * Results:
 *   P3_ALREADY_INITIALIZED:    this function has already been called
 *   P1_SUCCESS:                success
 *
 *----------------------------------------------------------------------
 */
int
P3SwapInit(int pages, int frames)
{
    int result = P1_SUCCESS;

    if(initialized){
        result = P3_ALREADY_INITIALIZED;
    }else{
        num_pages = pages;
        // Initializing Semaphores
        char name_swap[P1_MAXNAME + 1];
        strcpy(name_swap,"swap_sem");
        char name_vm[P1_MAXNAME + 1];
        strcpy(name_vm,"vmStat_sem");
        assert(P1_SemCreate(name_swap,1,&semSwap) == P1_SUCCESS);
        assert(P1_SemCreate(name_vm,1,&semVMStats) == P1_SUCCESS);

        int i;
        for(i = 0; i < P1_MAXPROC; i++){
            processes[i].block = malloc(pages * sizeof(Block));
            int j;
            for(j = 0; j < pages; j++){
                processes[i].block[j].track = -1;
                processes[i].block[j].swap = 0;
            }
        }

        // initialize the swap data structures, e.g. the pool of free blocks
        frame_processes.pid = malloc(sizeof(int) * frames);
        frame_processes.pages = malloc(sizeof(int) * frames);
        for(i = 0; i < frames; i ++){
            frame_processes.pid[i] = -1;
            frame_processes.pages[i] = -1;
        }

        assert(P1_P(semVMStats) == P1_SUCCESS);
        assert(P1_V(semVMStats) == P1_SUCCESS);
        initialized = 1;
    }

    return result;
}
/*
 *----------------------------------------------------------------------
 *
 * P3SwapShutdown --
 *
 *  Cleans up the swap data structures.
 *
 * Results:
 *   P3_NOT_INITIALIZED:    P3SwapInit has not been called
 *   P1_SUCCESS:            success
 *
 *----------------------------------------------------------------------
 */
int
P3SwapShutdown(void)
{
    int result = P1_SUCCESS;
    if(!initialized){
        result = P3_NOT_INITIALIZED;
    }else{
        // Frame structs
        free(frame_processes.pid);
        free(frame_processes.pages);
        int i;
        for(i = 0; i < P1_MAXPROC; i++){
            free(processes[i].block);
        }
        free(processes);

        // Free Semaphores
        assert(P1_SemFree(swap_sem) == P1_SUCCESS);
        assert(P1_SemFree(semVMStats) == P1_SUCCESS);
    }

    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * P3SwapFreeAll --
 *
 *  Frees all swap space used by a process
 *
 * Results:
 *   P3_NOT_INITIALIZED:    P3SwapInit has not been called
 *   P1_SUCCESS:            success
 *
 *----------------------------------------------------------------------
 */

int
P3SwapFreeAll(int pid)
{
    int result = P1_SUCCESS;

    kf(!initialized){
        result = P3_NOT_INITIALIZED;
    }else{
        /*****************

        P(mutex)
        free all swap space used by the process
        V(mutex)

        *****************/
        assert(P1_P(swap_sem) === P1_SUCCESS);
        int i;
        for(i = 0; i < num_pages; i++){
            processes[pid].block[i].track = -1;
            processes[pid].block[i].swap = 0;
        }
        assert(P1_V(swap_sem) == P1_SUCCESS);
    }
    


    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * P3SwapOut --
 *
 * Uses the clock algorithm to select a frame to replace, writing the page that is in the frame out 
 * to swap if it is dirty. The page table of the page’s process is modified so that the page no 
 * longer maps to the frame. The frame that was selected is returned in *frame. 
 *
 * Results:
 *   P3_NOT_INITIALIZED:    P3SwapInit has not been called
 *   P1_SUCCESS:            success
 *
 *----------------------------------------------------------------------
 */
int
P3SwapOut(int *frame) 
{
    int result = P1_SUCCESS;

    /*****************

    NOTE: in the pseudo-code below I used the notation frames[x] to indicate frame x. You 
    may or may not have an actual array with this name. As with all my pseudo-code feel free
    to ignore it.


    static int hand = -1;    // start with frame 0
    P(mutex)
    loop
        hand = (hand + 1) % # of frames
        if frames[hand] is not busy
            if frames[hand] hasn't been referenced (USLOSS_MmuGetAccess)
                target = hand
                break
            else
                clear reference bit (USLOSS_MmuSetAccess)
    if frame[target] is dirty (USLOSS_MmuGetAccess)
        write page to its location on the swap disk (P3FrameMap,P2_DiskWrite,P3FrameUnmap)
        clear dirty bit (USLOSS_MmuSetAccess)
    update page table of process to indicate page is no longer in a frame
    mark frames[target] as busy
    V(mutex)
    *frame = target

    *****************/

    return result;
}
/*
 *----------------------------------------------------------------------
 *
 * P3SwapIn --
 *
 *  Opposite of P3FrameMap. The frame is unmapped.
 *
 * Results:
 *   P3_NOT_INITIALIZED:     P3SwapInit has not been called
 *   P1_INVALID_PID:         pid is invalid      
 *   P1_INVALID_PAGE:        page is invalid         
 *   P1_INVALID_FRAME:       frame is invalid
 *   P3_EMPTY_PAGE:          page is not in swap
 *   P1_OUT_OF_SWAP:         there is no more swap space
 *   P1_SUCCESS:             success
 *
 *----------------------------------------------------------------------
 */
int
P3SwapIn(int pid, int page, int frame)
{
    int result = P1_SUCCESS;

    /*****************

    P(mutex)
    if page is on swap disk
        read page from swap disk into frame (P3FrameMap,P2_DiskRead,P3FrameUnmap)
    else
        allocate space for the page on the swap disk
        if no more space
            result = P3_OUT_OF_SWAP
        else
            result = P3_EMPTY_PAGE
    mark frame as not busy
    V(mutex)

    *****************/

    return result;
}