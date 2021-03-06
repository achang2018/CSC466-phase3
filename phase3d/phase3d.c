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

typedef struct Block {
    int track;
    int sector;
    int isSwapped;
} Block;

typedef struct Pages{
    Block *block;
} Pages;

typedef struct Frame{
    int pid;
    int page;
    int isBusy;
} Frame;

typedef struct Node {
    Block block;
    struct Node *next;
} Node;

static int initialized = 0;
static SID semSwap;
static SID semVMStats;
static Pages *processes;
static Node *freeBlocks; // A stack of the free blocks
static int num_pages;
static int num_frames;
static Frame *frame_processes;
static int sector_size;
static int num_sectors; // Number of sectors per track
static int num_tracks;  // Total number of tracks
static int sectors_per_page;


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
    USLOSS_Console("Swap Init Start\n");
    int result = P1_SUCCESS;

    if(initialized){
        result = P3_ALREADY_INITIALIZED;
    }else{
        num_pages = pages;
        num_frames = frames;
        freeBlocks = NULL;
        // Initializing Semaphores
        char name_swap[P1_MAXNAME + 1];
        strcpy(name_swap,"swap_sem");
        char name_vm[P1_MAXNAME + 1];
        strcpy(name_vm,"vmStat_sem");
        assert(P1_SemCreate(name_swap,1,&semSwap) == P1_SUCCESS);
        assert(P1_SemCreate(name_vm,1,&semVMStats) == P1_SUCCESS);

        // Initializing the disk
        assert(P2_DiskSize(1, &sector_size, &num_sectors, &num_tracks) == P1_SUCCESS);
        int pageSize = USLOSS_MmuPageSize();
        sectors_per_page = pageSize / sector_size;
        if (pageSize % sector_size != 0) {
            sectors_per_page++;
        }
        USLOSS_Console("Sectors per page %d and num sectors %d\n", sectors_per_page, num_sectors);
        int i; int j;
        int startJ = 0;
        // USLOSS_Console("Creating Free Block Space\n");
        // Giving space for the freeBlocks stack
        for (i=0; i<num_tracks; i++) {
            for (j=startJ; j<num_sectors; j+=sectors_per_page) {
                // USLOSS_Console("i %d j %d\n", i, j);
                Node *node = malloc(sizeof(Node));
                node->block.sector = j;
                node->block.track = i;
                node->block.isSwapped = FALSE;
                node->next = freeBlocks;
                freeBlocks = node;
            }
            startJ = j - num_sectors;
        }
        USLOSS_Console("Setting up proc ds\n");
        processes = malloc(P1_MAXPROC * sizeof(Pages));
        for(i = 0; i < P1_MAXPROC; i++){
            processes[i].block = malloc(pages * sizeof(Block));
            int j;
            for(j = 0; j < pages; j++) {
                processes[i].block[j].track = -1;
                processes[i].block[j].isSwapped = FALSE;
                processes[i].block[j].sector = -1;
            }
        }

        // initialize the swap data structures, e.g. the pool of free blocks
        USLOSS_Console("Initializing frames\n");
        frame_processes = malloc(sizeof(Frame) * frames);
        for(i = 0; i < frames; i ++){
            frame_processes[i].pid = -1;
            frame_processes[i].page = -1;
            frame_processes[i].isBusy = FALSE;

        }

        assert(P1_P(semVMStats) == P1_SUCCESS);
        assert(P1_V(semVMStats) == P1_SUCCESS);
        initialized = 1;
    }
    USLOSS_Console("SwapInit End\n");
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
    USLOSS_Console("Swap Shutdown start\n");
    int result = P1_SUCCESS;
    if(!initialized){
        result = P3_NOT_INITIALIZED;
    }else{
        // Frame structs
        int i;
        for (i=0; i<num_frames; i++) {
            Frame *ptr = frame_processes + i;
            free(ptr);
        }

        for(i = 0; i < P1_MAXPROC; i++){
            free(processes[i].block);
        }
        free(processes);

        // Free Semaphores
        assert(P1_SemFree(semSwap) == P1_SUCCESS);
        assert(P1_SemFree(semVMStats) == P1_SUCCESS);
    }
    USLOSS_Console("Swap Shutdown End\n");
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

    if(!initialized){
        result = P3_NOT_INITIALIZED;
    }else{
        /*****************

        P(mutex)
        free all swap space used by the process
        V(mutex)

        *****************/
        assert(P1_P(semSwap) == P1_SUCCESS);
        int i;
        for(i = 0; i < num_pages; i++){
            if (processes[pid].block[i].track != -1 && processes[pid].block[i].sector != -1) {
                Node *node = malloc(sizeof(Node));
                node->block.isSwapped = FALSE;
                node->block.sector = processes[pid].block[i].sector;
                node->block.track = processes[pid].block[i].track;
                node->next = freeBlocks;
                freeBlocks = node;
                processes[pid].block[i].track = -1;
                processes[pid].block[i].sector = -1;
                processes[pid].block[i].isSwapped = FALSE;
            }
        }
        assert(P1_V(semSwap) == P1_SUCCESS);
        
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
   assert(P1_P(semSwap) == P1_SUCCESS);
   USLOSS_Console("SwapOut Start\n");
   if (!initialized) {
       return P3_NOT_INITIALIZED;
   }
   static int hand = -1;
   int access; int target;
   while (TRUE) {
       hand = (hand + 1) % num_frames;
       USLOSS_Console("Looking at frame %d\n", hand);
       assert(USLOSS_MmuGetAccess(hand, &access) == USLOSS_MMU_OK);
       if (access != USLOSS_MMU_REF) {
           target = hand;
           break;
       } else {
           assert(USLOSS_MmuSetAccess(hand, 0) == USLOSS_MMU_OK);
       }
   }
   int pid = frame_processes[target].pid;
   int page = frame_processes[target].page;
   // Writing to disk if the frame is dirty
   if (access == USLOSS_MMU_DIRTY) {
       USLOSS_Console("Swapping Dirty\n");
       void *ptr;
       assert(P3FrameMap(target, &ptr) == P1_SUCCESS);
       int sector = processes[pid].block[page].sector;
       int track = processes[pid].block[page].track;
       USLOSS_Console("Writing for sector %d for track %d\n", sector, track);
       assert(P2_DiskWrite(1, track, sector, sectors_per_page, ptr) == P1_SUCCESS);
       assert(P3FrameUnmap(target) == P1_SUCCESS);
   }
    // setting incore to 0 for the page in the page table
    USLOSS_PTE *table;
    assert(P3PageTableGet(pid, &table) == P1_SUCCESS);
    table[page].incore = 0;
    table[page].read = 0;
    table[page].write = 0;
    USLOSS_Console("Setting table\n");
    assert(USLOSS_MmuSetPageTable(table) == USLOSS_MMU_OK);
    frame_processes[target].isBusy = TRUE;
    assert(P1_V(semSwap) == P1_SUCCESS);
    *frame = target;
    USLOSS_Console("Swap Out End\n");
    return P1_SUCCESS;
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
    USLOSS_Console("SwapIn Start\n");
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
   if (!initialized) {
       return P3_NOT_INITIALIZED;
   }
   if (pid < 0 || pid >= P1_MAXPROC) {
       return P1_INVALID_PID;
   }
   if (page < 0 || page >= num_pages) {
       USLOSS_Console("Invalid page %d\n", page);
       return P3_INVALID_PAGE;
   }
   if (frame < 0 || frame >= num_frames) {
       USLOSS_Console("Invalid Frame %d\n", frame);
       return P3_INVALID_FRAME;
   }

   int ret = P1_SUCCESS;
    assert(P1_P(semSwap) == P1_SUCCESS);
    if (processes[pid].block[page].track != -1 && processes[pid].block[page].sector != -1) {
        void *ptr;
        int pageSize = USLOSS_MmuPageSize();
        assert(P3FrameMap(frame, &ptr) == P1_SUCCESS);
        USLOSS_Console("Reading for pid %d, track %d and sector %d\n", pid, processes[pid].block[page].track, processes[pid].block[page].sector);
        char *addr = malloc(pageSize);
        assert(P2_DiskRead(1, processes[pid].block[page].track, processes[pid].block[page].sector, sectors_per_page, addr) == P1_SUCCESS);
        int i;
        for (i=0; i<pageSize; i++) {
            ((char *)ptr)[i] = addr[i];
        }
        free(addr);
        assert(P3FrameUnmap(frame) == P1_SUCCESS);
        USLOSS_Console("Finished Reading\n");
    } else {
        Node *node = freeBlocks;
        if (node == NULL) {
            ret = P3_OUT_OF_SWAP;

        } else {
            USLOSS_Console("Giving track %d and sector %d to %d\n", node->block.track, node->block.sector, pid);
            processes[pid].block[page].track = node->block.track;
            processes[pid].block[page].sector = node->block.sector;
            freeBlocks = freeBlocks->next;
            free(node);
            ret = P3_EMPTY_PAGE;
        }
    }
    frame_processes[frame].isBusy = FALSE;
    frame_processes[frame].pid = pid;
    frame_processes[frame].page = page;
    USLOSS_Console("SwapIn end\n");
    assert(P1_V(semSwap) == P1_SUCCESS);
    return ret;
}