/*
 * paging.c
 *  
 *  Basic test case for Phase 3c. It creates two processes, "A" and "B". 
 *  Each process has four pages and there are CHILDREN * PAGES frames.

 *  Each process writes its name into the first byte of each of its pages, sleeps for one
 *  second (to give the other process time to run), then verifies that the first byte
 *  of each page is correct. It then iterates a fixed number of times.
 *
 *  You can change the number of pages and iterations by changing the macros below. You
 *  can add more processes by adding more names to the "names" array, e.g. "C". The
 *  code will adjust the number of frames accordingly.
 *
 *  It makes liberal use of the "assert" function because it will dump core when it fails
 *  allowing you to easily look at the state of the program and figure out what went wrong,
 *  and because I'm lazy.
 *
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

#define CHILDREN (sizeof(names) / sizeof(char *))
#define PAGES 4 // # of pages per child
#define FRAMES (CHILDREN * PAGES)
#define PRIORITY 3
#define ITERATIONS 100
#define PAGERS 3

static char *vmRegion;
static char *names[] = {"A","B","C"};
static int  pageSize;
static int passed = FALSE;
#ifdef DEBUG
int debugging = 1;
#else
int debugging = 1;
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
Child(void *arg)
{

    volatile char *name = (char *) arg;
    int     i,j;
    char    *addr;
    int     tod;
    int     rc;
    Debug("Child \"%s\" starting.\n", name);
    for (i = 0; i < ITERATIONS; i++) {
        for (j = 0; j < PAGES; j++) {
            addr = vmRegion + j * pageSize;
            Sys_GetTimeOfDay(&tod);
            Debug("%f: Child \"%s\" writing to page %d @ %p\n", tod / 1000000.0, name, j, addr);
            *addr = *name;
        }
        rc = Sys_Sleep(1);
        assert(rc == P1_SUCCESS);
        for (j = 0; j < PAGES; j++) {
            addr = vmRegion + j * pageSize;
            Sys_GetTimeOfDay(&tod);
            Debug("%f: Child \"%s\" reading from page %d @ %p\n", tod / 1000000.0, name, j, addr);
            assert(*addr == *name);
        }
    }
    Debug("Child \"%s\" done.\n", name);
    return 0;

}


int
P4_Startup(void *arg)
{
    int     i;
    int     rc;
    int     pid;
    int     child;

    Debug("P4_Startup starting.\n");
    rc = Sys_VmInit(PAGES, PAGES, FRAMES, PAGERS, (void **) &vmRegion);
    if (rc != 0) {
        USLOSS_Console("Sys_VmInit failed: %d\n", rc);
        USLOSS_Halt(1);
    }
    pageSize = USLOSS_MmuPageSize();
    for (i = 0; i < CHILDREN; i++) {
        rc = Sys_Spawn(names[i], Child, (void *) names[i], USLOSS_MIN_STACK * 2, PRIORITY, &pid);
        assert(rc == P1_SUCCESS);
    }
    for (i = 0; i < CHILDREN; i++) {
        rc = Sys_Wait(&pid, &child);
        assert(rc == P1_SUCCESS);
        TEST(child, 0);
    }
    Sys_VmShutdown();
    Debug("P4_Startup done.\n");
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
int P3SwapIn(PID pid, int page, int frame) {return P3_EMPTY_PAGE;}



