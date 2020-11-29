/*
 * phase3c.c
 * 
 * Authors: Bianca Lara and Ann Chang
 *
 */

#include <assert.h>
#include <phase1.h>
#include <phase2.h>
#include <usloss.h>
#include <string.h>
#include <libuser.h>

#include "phase3.h"
#include "phase3Int.h"

#ifdef DEBUG
int debugging3 = 1;
#else
int debugging3 = 1;
#endif

void debug3(char *fmt, ...)
{
    va_list ap;

    if (debugging3) {
        va_start(ap, fmt);
        USLOSS_VConsole(fmt, ap);
    }
}

static int FRAME_UNUSED = 1;
static int FRAME_MAPPED = 2;

typedef struct Frame
{
    PID pid;
    int state;
} Frame;

typedef struct PagerStruct {
    PID pid;
    SID sid;
    int quit;
} PagerStruct;

static int Pager(void *arg);
static PagerStruct pagersList[P3_MAX_PAGERS];
static int initialized;
static int numPages;

static Frame *framesList;
static SID frameSem;
static SID vmStatsSem;

static void IllegalMessage(int n, void *arg){
    P1_Quit(1024);
}

static void checkInKernelMode() {
    if(!(USLOSS_PsrGet() & USLOSS_PSR_CURRENT_MODE)){
        USLOSS_IntVec[USLOSS_ILLEGAL_INT] = IllegalMessage;
        USLOSS_IllegalInstruction();
    }
}

/*
 *----------------------------------------------------------------------
 *
 * P3FrameInit --
 *
 *  Initializes the frame data structures.
 *  Four states of a frame:
 *      FRAME_UNUSED
 *      FRAME_INUSE
 *      FRAME_ASSIGNED
 *      FRAME_MAPPED
 *
 * Results:
 *   P3_ALREADY_INITIALIZED:    this function has already been called
 *   P1_SUCCESS:                success
 *
 *----------------------------------------------------------------------
 */
int
P3FrameInit(int pages, int frames)
{
    int result = P1_SUCCESS;
    checkInKernelMode();
    
    if(framesList == NULL) {
        // Storing the total number of pages
        numPages = pages;
        // creating semaphore for frames
        char frameSemName[P1_MAXNAME];
        strcpy(frameSemName,"frameSem");
        assert(P1_SemCreate(frameSemName, 1, &frameSem) == P1_SUCCESS);
        assert(P1_P(frameSem) == P1_SUCCESS);

        // initialize the frame data structures, e.g. the pool of  frames
        framesList = malloc(sizeof(Frame) * frames);
        int i;
        for(i = 0; i < frames; i++){
            framesList[i].state = FRAME_UNUSED;
            framesList[i].pid = -1;
        }
        assert(P1_V(frameSem) == P1_SUCCESS);

        // creating semaphore for vmStats
        char vmStatsName[P1_MAXNAME];
        strcpy(vmStatsName, "vmStatsSem");
        assert(P1_SemCreate(vmStatsName, 1, &vmStatsSem) == P1_SUCCESS);
        assert(P1_P(vmStatsSem) == P1_SUCCESS);

        // set P3_vmStats.freeFrames
        P3_vmStats.freeFrames = frames;
        P3_vmStats.frames = frames;

        assert(P1_V(vmStatsSem) == P1_SUCCESS);

    } else {
        result = P3_ALREADY_INITIALIZED;
    }

    return result;
}
/*
 *----------------------------------------------------------------------
 *
 * P3FrameShutdown --
 *
 *  Cleans up the frame data structures.
 *
 * Results:
 *   P3_NOT_INITIALIZED:    P3FrameInit has not been called
 *   P1_SUCCESS:            success
 *
 *----------------------------------------------------------------------
 */
int
P3FrameShutdown(void)
{
    int result = P1_SUCCESS;
    checkInKernelMode();

    if(framesList == NULL){
        result = P3_NOT_INITIALIZED;
    } else {
        // free frameList
        assert(P1_P(frameSem) == P1_SUCCESS);
        free(framesList);
        framesList = NULL;
        // Free Semaphores for frame and vmStats
        assert(P1_V(frameSem) == P1_SUCCESS);
        assert(P1_SemFree(frameSem) == P1_SUCCESS);
        assert(P1_SemFree(vmStatsSem) == P1_SUCCESS);
    }

    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * P3FrameFreeAll --
 *
 *  Frees all frames used by a process
 *
 * Results:
 *   P3_NOT_INITIALIZED:    P3FrameInit has not been called
 *   P1_SUCCESS:            success
 *
 *----------------------------------------------------------------------
 */

int
P3FrameFreeAll(int pid)
{
    USLOSS_PTE *table;
    // free all frames in use by the process (P3PageTableGet)
    int ret = P3PageTableGet(pid, &table);
    if (ret != P1_SUCCESS || table == NULL) {
        return P3_NOT_INITIALIZED;
    }
    int i;
    for (i =0; i<numPages; i++) {  
        if (table[i].incore == 1) {
            framesList[table[i].frame].state = FRAME_UNUSED;
            framesList[table[i].frame].pid = -1;
            table[i].incore = 0;
            table[i].frame = -1;
            table[i].read = 0;
            table[i].write = 0;
        }
    }
    P3_vmStats.freeFrames = P3_vmStats.frames;
    ret = USLOSS_MmuSetPageTable(table);
    assert(ret == USLOSS_MMU_OK);
    return P1_SUCCESS;
}

/*
 *----------------------------------------------------------------------
 *
 * P3FrameMap --
 *
 *  Maps a frame to an unused page and returns a pointer to it.
 *
 * Results:
 *   P3_NOT_INITIALIZED:    P3FrameInit has not been called
 *   P3_OUT_OF_PAGES:       process has no free pages
 *   P1_INVALID_FRAME       the frame number is invalid
 *   P1_SUCCESS:            success
 *
 *----------------------------------------------------------------------
 */
int
P3FrameMap(int frame, void **ptr) 
{
    USLOSS_PTE *table;
    if (frame < 0 || frame >= P3_vmStats.frames || framesList[frame].state != FRAME_UNUSED) {
        return P3_INVALID_FRAME;
    }
    // get the page table for the process (P3PageTableGet)
    int ret = P3PageTableGet(framesList[frame].pid, &table);
    if (ret != P1_SUCCESS || table == NULL) {
        return P3_NOT_INITIALIZED;
    }
    int pages;
    *ptr = USLOSS_MmuRegion(&pages);
    int pageSize = USLOSS_MmuPageSize();

    // find an unused page
    int i;
    for (i=0; i<pages; i++) {
        if (table[i].incore == 0) {
            // update the page's PTE to map the page to the frame
            table[i].frame = frame;
            table[i].incore = 1;
            table[i].read = 1;
            table[i].write = 1;

            framesList[frame].state = FRAME_MAPPED;
            // Moving the ptr to the page that we found
            *ptr += i * pageSize; 
            // Update the page table in the MMU (USLOSS_MmuSetPageTable)
            ret = USLOSS_MmuSetPageTable(table);
            assert(ret == USLOSS_MMU_OK);
            assert(P3PageTableSet(P1_GetPid(), table) == P1_SUCCESS);

            P3_vmStats.freeFrames -= 1;

            return P1_SUCCESS;
        }
    }
    return P3_OUT_OF_PAGES;
}
/*
 *----------------------------------------------------------------------
 *
 * P3FrameUnmap --
 *
 *  Opposite of P3FrameMap. The frame is unmapped.
 *
 * Results:
 *   P3_NOT_INITIALIZED:    P3FrameInit has not been called
 *   P3_FRAME_NOT_MAPPED:   process didn’t map frame via P3FrameMap
 *   P1_INVALID_FRAME       the frame number is invalid
 *   P1_SUCCESS:            success
 *
 *----------------------------------------------------------------------
 */
int
P3FrameUnmap(int frame) 
{
    USLOSS_PTE *table;
    if (frame < 0 || frame >= P3_vmStats.frames) {
        return P3_INVALID_FRAME;
    }
    // get the process's page table (P3PageTableGet)
    int ret = P3PageTableGet(framesList[frame].pid, &table);
    if (ret != P1_SUCCESS || table == NULL) {
        return P3_NOT_INITIALIZED;
    }
    // verify that the process mapped the frame
    int i;
    for (i=0; i<numPages; i++) {
        if (table[i].incore == 1 && table[i].frame == frame) {
            // update page's PTE to remove the mapping
            table[i].incore = 0;
            table[i].read = 0;
            table[i].write = 0;
            framesList[frame].state = FRAME_UNUSED;
            P3_vmStats.freeFrames += 1;
            // update the page table in the MMU (USLOSS_MmuSetPageTable);
            ret = USLOSS_MmuSetPageTable(table);
            assert(ret == USLOSS_MMU_OK);
            return P1_SUCCESS;
        }
    }
    return P3_FRAME_NOT_MAPPED;
}

// information about a fault. Add to this as necessary.

typedef struct Fault {
    PID         pid;
    int         offset;
    int         cause;
    SID         wait;
    // other stuff goes here
    struct Fault*       next; //The next fault in the linked list
    int         status;
} Fault;
static Fault *faultHead;
static Fault *faultTail;
static SID    faultSem;
static SID    faultListSem;
static int numPagers;
// static SID pagerSem;

/*
 *----------------------------------------------------------------------
 *
 * FaultHandler --
 *
 *  Page fault interrupt handler
 *
 *----------------------------------------------------------------------
 */

static void
FaultHandler(int type, void *arg)
{
    
    Fault*   fault = malloc(sizeof(Fault));
    // mutex for vmStats
    assert(P1_P(vmStatsSem) == P1_SUCCESS);
    P3_vmStats.faults += 1;
    assert(P1_V(vmStatsSem) == P1_SUCCESS);

    assert(P1_P(faultListSem) == P1_SUCCESS);
    // fill in other fields in fault
    fault->pid = P1_GetPid();
    fault->offset = (int) arg;
    fault->cause = USLOSS_MmuGetCause();
    fault->wait = 0;
    if (fault->cause == USLOSS_MMU_ERR_ACC) {
        P2_Terminate(USLOSS_MMU_ERR_ACC);
    }
    // add to queue of pending faults
    if (faultHead == NULL) {
        faultHead = fault;
        faultTail = fault;
    } else {
        faultTail->next = fault;
        faultTail = faultTail->next;
    }
    // Let the pagers know there is a pending fault
    
    // USLOSS_Console("Fault Detected!\n");
    assert(P1_V(faultSem) == P1_SUCCESS); // Notifying the pagers that a fault has ocurred
    // wait for fault to be handled
    if (faultHead->status == P3_OUT_OF_SWAP) {
        P2_Terminate(P3_OUT_OF_SWAP);
    }
    if (faultHead == faultTail) {
                free(faultHead);
                faultHead = NULL;
                faultTail = NULL;
    } else {
        Fault *fault = faultHead;
        faultHead = faultHead->next;
        free(fault);
    }
    assert(P1_V(faultListSem) == P1_SUCCESS);
    // USLOSS_Console("Fault Finished\n");
}


/*
 *----------------------------------------------------------------------
 *
 * P3PagerInit --
 *
 *  Initializes the pagers.
 *
 * Results:
 *   P3_ALREADY_INITIALIZED: this function has already been called
 *   P3_INVALID_NUM_PAGERS:  the number of pagers is invalid
 *   P1_SUCCESS:             success
 *
 *----------------------------------------------------------------------
 */
int
P3PagerInit(int pages, int frames, int pagers)
{
    int     result = P1_SUCCESS;
    numPagers = pagers;
    checkInKernelMode();
    USLOSS_IntVec[USLOSS_MMU_INT] = FaultHandler;

    // creating semaphore for the fault handler to switch to the pagers
    char faultSemName[P1_MAXNAME];
    strcpy(faultSemName, "faultSem");
    assert(P1_SemCreate(faultSemName, 0, &faultSem) == P1_SUCCESS);
    char faultListSemName[P1_MAXNAME];
    strcpy(faultListSemName, "faultList");
    assert(P1_SemCreate(faultListSemName, 1, &faultListSem) == P1_SUCCESS);

    if(initialized){
        result = P3_ALREADY_INITIALIZED;
    } else if (pagers <= 0 || pagers > P3_MAX_PAGERS){
        result = P3_INVALID_NUM_PAGERS;
    }else{
        // initialize the pager data structures
        initialized = 1;
        int i;
        for(i = 0; i < P3_MAX_PAGERS; i++){
            pagersList[i].pid = -1;
            pagersList[i].sid = -1;
            pagersList[i].quit = 0;
        }

        // fork off the pagers and wait for them to start running
        for(i = 0; i < pagers; i++){
            char name[P1_MAXNAME + 1];
            snprintf(name,sizeof(name),"%s%d","pager",i);
            assert(P1_SemCreate(name,0,&pagersList[i].sid) == P1_SUCCESS);

            int pid;
            assert(P1_Fork(name, Pager,&i,USLOSS_MIN_STACK,P3_PAGER_PRIORITY,1,&pid) == P1_SUCCESS);
            pagersList[i].pid = pid;

            assert(P1_P(pagersList[i].sid) == P1_SUCCESS);
            // assert(P1_V(pagersList[i].sid) == P1_SUCCESS);
        }
        
    }
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * P3PagerShutdown --
 *
 *  Kills the pagers and cleans up.
 *
 * Results:
 *   P3_NOT_INITIALIZED:     P3PagerInit has not been called
 *   P1_SUCCESS:             success
 *
 *----------------------------------------------------------------------
 */
int
P3PagerShutdown(void)
{
    int result = P1_SUCCESS;
    if(!initialized){
        result = P3_NOT_INITIALIZED;
    }else{
        int i;      
        initialized = 0;
        
        // cause the pagers to quit
        for(i = 0; i < P3_MAX_PAGERS; i++){
            if(pagersList[i].quit != -1){
                USLOSS_Console("Pager Shutdowns\n");
                // setting the pagerList[i] to quit
                pagersList[i].quit = 1;
                assert(P1_V(faultSem) == P1_SUCCESS); // Shutting down the pagers
            }
        }

        // clean up the pager data structures
    }

    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * Pager --
 *
 *  Handles page faults
 *
 *----------------------------------------------------------------------
 */

static int
Pager(void *arg)
{
    USLOSS_Console("Pager Started\n");
    int pagerCount = *((int *)arg);
    //  notify P3PagerInit that we are running
    assert(P1_V(pagersList[pagerCount].sid) == P1_SUCCESS);

    // loop until P3PagerShutdown is called
    while(initialized != 0) {
        USLOSS_Console("Waiting for fault..\n");
        assert(P1_P(faultSem) == P1_SUCCESS);
        USLOSS_Console("Found a fault!\n");
        if (faultHead != NULL) {
            int frame;
            if (P3_vmStats.freeFrames > 0) {
                int i;
                for (i=0; i<P3_vmStats.frames; i++) {
                    if (framesList[i].state == FRAME_UNUSED) {
                        frame = i;
                        break;
                    }
                }
            } else {
                assert(P3SwapOut(&frame) == P1_SUCCESS);
                framesList[frame].state = FRAME_UNUSED;
            }
            // USLOSS_Console("Using the frame %d\n", frame);
            framesList[frame].pid = faultHead->pid;
            int pageSize = USLOSS_MmuPageSize();
            int page = faultHead->offset/pageSize;
            int ret = P3SwapIn(faultHead->pid, page, frame);
            if (ret == P3_EMPTY_PAGE) {
                // New page, add to vmStats
                assert(P1_P(vmStatsSem) == P1_SUCCESS);
                P3_vmStats.new += 1;
                assert(P1_V(vmStatsSem) == P1_SUCCESS);
                void *addr;
                assert(P3FrameMap(frame, &addr) == P1_SUCCESS);
                // Zero out the frame at the given address
                memset(addr, 0, pageSize);
                assert(P3FrameUnmap(frame) == P1_SUCCESS);
            } else if (ret == P3_OUT_OF_SWAP) {
                faultHead->status = P3_OUT_OF_SWAP;
                continue;
            }
            void *ptr;
            // update PTE in faulting process's page table to map page to frame
            assert(P3FrameMap(frame, &ptr) == P1_SUCCESS);
            faultHead->status = P1_SUCCESS;
            // assert(P1_V(faultHead->wait) == P1_SUCCESS);
        }
    }
    USLOSS_Console("Pager Finished\n");
    return 0;
}

    /********************************

    notify P3PagerInit that we are running
    loop until P3PagerShutdown is called
        wait for a fault
        if it's an access fault kill the faulting process
        if there are free frames
            frame = a free frame
        else
            P3SwapOut(&frame);
        rc = P3SwapIn(pid, page, frame)
        if rc == P3_EMPTY_PAGE
            P3FrameMap(frame, &addr)
            zero-out frame at addr
            P3FrameUnmap(frame);
        else if rc == P3_OUT_OF_SWAP
            kill the faulting process
        update PTE in faulting process's page table to map page to frame
        unblock faulting process

    **********************************/
