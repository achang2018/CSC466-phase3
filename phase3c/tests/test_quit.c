/*
 * test_quit.c
 * Have the child process touch a page which will cause a page fault and a page-in
 * The child should die since we have an out of swap error
 */
#include <usyscall.h>
#include <libuser.h>
#include <assert.h>
#include <usloss.h>
#include <stdlib.h>
#include <phase3.h>
#include <stdarg.h>
#include <unistd.h>

#include "tester.h"
#include "phase3Int.h"

#define PAGES 2             // # of pages
#define FRAMES 2            // # of frames
#define PAGERS 2            // # of pagers

#define PAGE_ADDR(page, mmuPageSize) ((char *) (vmRegion + ((page) * mmuPageSize)))

static char *vmRegion;
static int  pageSize;
static int  parentPID;
static int  sem;
static int passed = FALSE;

#ifdef DEBUG
int debugging = 1;
#else
int debugging = 0;
#endif /* DEBUG */

static void
Debug(char *fmt, ...)
{
    va_list ap;

    if (debugging) {
        va_start(ap, fmt);
        USLOSS_VConsole(fmt, ap);
    }
}
static int
Child(void *arg){
    USLOSS_Console("Child starting\n");
    
    int rc;
    char    dummy;
    rc = Sys_SemP(sem);
    TEST(rc, P1_SUCCESS);
    USLOSS_Console("Child continuing\n");
    // Touch a page
    int ps = (int)USLOSS_MmuPageSize();
    dummy = * ((int *) PAGE_ADDR(0, ps));
    
    // Should not get here.
    passed = FALSE;
    USLOSS_Console("Child still alive!!\n");
    return 1;
}


static int Parent(void *arg){
    USLOSS_Console("Parent starting\n");
    int   pid;
    int   rc;

    rc = Sys_Spawn("Child", Child, NULL, USLOSS_MIN_STACK * 4 , 3, &pid);
    USLOSS_Console("Parent continuing\n");

    rc = Sys_SemV(sem);
    TEST(rc, P1_SUCCESS);
    USLOSS_Console("Parent terminating\n");
    return 1; 
}
int P4_Startup(void *arg)
{
    int     rc;
    int     pid;
    int     status;
    
    Debug("P4_Startup starting.\n");
    rc = Sys_VmInit(PAGES, PAGES, FRAMES, PAGERS, (void **) &vmRegion);
    TEST(rc, P1_SUCCESS);
    rc = Sys_SemCreate("sem", 0, &sem);
    TEST(rc, P1_SUCCESS);
    pageSize = USLOSS_MmuPageSize();
    rc = Sys_Spawn("Parent", Parent, NULL, USLOSS_MIN_STACK * 4, 4, &parentPID);
    assert(rc == P1_SUCCESS);
    rc = Sys_Wait(&pid, &status);
    assert(rc == P1_SUCCESS);
    TEST(status, 1);
    Debug("Child terminated\n");
    Sys_VmShutdown();
    PASSED();
    return 0;
}


void test_setup(int argc, char **argv) {
}

void test_cleanup(int argc, char **argv) {
    if (passed) {
        USLOSS_Console("TEST PASSED.\n");
    }
}

// Phase 3d stubs

#include "phase3Int.h"

int P3SwapInit(int pages, int frames) {return P1_SUCCESS;}
int P3SwapShutdown(void) {return P1_SUCCESS;}
int P3SwapFreeAll(PID pid) {return P1_SUCCESS;}
int P3SwapOut(int *frame) {return P1_SUCCESS;}
int P3SwapIn(PID pid, int page, int frame) {return P3_OUT_OF_SWAP;}



