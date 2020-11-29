/*
 * Zero.c
 *  
 *  Zero test case for Phase 3 milestone. It creates two processes, "A" and "B". 
 *  Each process has two pages and there are four frames so that all pages fit in memory.
 *  Each process verifies that its pages are zero-filled.
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
#include "phase3Int.h"
#define PAGES 2
#define ITERATIONS 10

static char *vmRegion;
static char *names[] = {"A","B"};
static int  pageSize;
static int passed = FALSE;

#ifdef DEBUG
int debugging = 1;
#else
int debugging = 0;
#endif /* DEBUG */

static void
debug(char *fmt, ...)
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
    char    *name = (char *) arg;
    int     i,j,k;
    char    *page;
    int     rc;
    debug("Child \"%s\" starting.\n", name);
    for (i = 0; i < ITERATIONS; i++) {
        for (j = 0; j < PAGES; j++) {
            page = (char *) (vmRegion + j * pageSize);
            for (k = 0; k < pageSize; k++) {
                assert(page[k] == '\0');
            }
        }
    rc = Sys_Sleep(1);
    assert(rc == P1_SUCCESS);
    }
    debug("Child \"%s\" done.\n", name);
    return 0;
}


int
P4_Startup(void *arg)
{
    int     i;
    int     rc;
    int     pid;
    int     child;
    int     numChildren = sizeof(names) / sizeof(char *);

    debug("P4_Startup starting.\n");
    rc = Sys_VmInit(PAGES, PAGES, numChildren * PAGES, 1, (void **) &vmRegion);
    if (rc != 0) {
        USLOSS_Console("Sys_VmInit failed: %d\n", rc);
        USLOSS_Halt(1);
    }
    pageSize = USLOSS_MmuPageSize();
    for (i = 0; i < numChildren; i++) {
        rc = Sys_Spawn(names[i], Child, (void *) names[i], USLOSS_MIN_STACK * 2, 2, &pid);
        assert(rc == 0);
    }
    for (i = 0; i < numChildren; i++) {
        rc = Sys_Wait(&pid, &child);
        assert(rc == 0);
        TEST(child, 0);
    }
    Sys_VmShutdown();
    PASSED();
    debug("P4_Startup done.\n");
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


