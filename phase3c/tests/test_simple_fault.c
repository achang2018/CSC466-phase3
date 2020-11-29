/*
 * test_simple_fault.c
 *  There should be only 2 page faults as subsequent faults do not count yet when a page is accessed.
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

#define PAGES 2         // # of pages per process (be sure to try different values)
#define FRAMES (PAGES)
#define ITERATIONS 1
#define PAGERS 1        // # of pagers

static char *vmRegion;
static char *names[] = {"A","B"};   // names of children, add more names to create more children
static int  pageSize;

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
Child(void *arg)
{
    volatile char *name = (char *) arg;
    char    *page;
    char    *page2;
    char    message1[] = "Something for page1";  
    char    message2[] = "Something for page2";
    int     pid;
    
 
    Sys_GetPID(&pid);
    Debug("Child \"%s\" (%d) starting.\n", name, pid);

    // The first time a page is read it should be full of zeros.
    for (int j = 0; j < PAGES; j++) {
        page = vmRegion + j * pageSize;
        Debug("Child \"%s\" reading zeros from page %d @ %p\n", name, j, page);
        for (int k = 0; k < pageSize; k++) {
            TEST(page[k], '\0');
        }
    }    
    P3_PrintStats(&P3_vmStats);
    
    page = (char *)(vmRegion);
    page2 = (char *)(vmRegion + pageSize);

    strcpy(page, message1);
    strcpy(page2, message2);

    if(strcmp(page, message1) == 0){
       PASSED();
    }
    P3_PrintStats(&P3_vmStats);
    Debug("Child \"%s\" done.\n", name);
    return 0;
}


int
P4_Startup(void *arg)
{
    int     i;
    int     rc;
    int     pid;
    int     status;
    int     numChildren = sizeof(names) / sizeof(char *);

    numChildren = 1;
    Debug("P4_Startup starting.\n");
    rc = Sys_VmInit(PAGES, PAGES, FRAMES, PAGERS, (void **) &vmRegion);
    TEST(rc, P1_SUCCESS);


    pageSize = USLOSS_MmuPageSize();
    for (i = 0; i < numChildren; i++) {
        rc = Sys_Spawn(names[i], Child, (void *) names[i], USLOSS_MIN_STACK * 4, 3, &pid);
        assert(rc == P1_SUCCESS);
    }
    for (i = 0; i < numChildren; i++) {
        rc = Sys_Wait(&pid, &status);
        assert(rc == P1_SUCCESS);
        TEST(status, 0);
    }
    Debug("Children terminated\n");
    Sys_VmShutdown();
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
