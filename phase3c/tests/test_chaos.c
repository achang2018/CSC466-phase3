/*
 * test_chaos.c
 *  
 *  Stress test case for Phase 3c. It creates 10 processes, Child. 
 *  Each Child writes to its pages and reads what was written at random.
 *
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
#include <string.h>

#include "tester.h"

#define NUMCHILDREN 10
#define DEBUG 1
#define PAGES 4        
#define ITERATIONS 100
#define FRAMES NUMCHILDREN * PAGES
#define PAGERS 2
#define Rand(limit) ((int) ((((double)((limit)+1)) * rand()) / \
            ((double) RAND_MAX)))


static char *vmRegion;
static int  pageSize;

#ifdef DEBUG
int debugging = 1;
#else
int debugging = 1;
#endif /* DEBUG */


char *format = "Child: %d, page:%d";
static int passed = FALSE;

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
    int     id = (int) arg;
    int     pid;
    char    buffer[128];
    char    empty[128] = "";
    int     action;
    int     valid[PAGES];
    Sys_GetPID(&pid);
    Debug("Child (%d) starting.\n", pid);

    for(int j =0; j < PAGES; j++){
       char *page = (char *) (vmRegion + j * pageSize);
       USLOSS_Console("Beginning string copy at %p\n", page);
       strcpy(page, empty); 	
       USLOSS_Console("Done with string copy!\n");
       valid[j] = 0;
    }
    USLOSS_Console("Starting iterations\n");
    for(int k = 0; k < ITERATIONS; k++){ 
       for (int i = 0; i < PAGES; i++) {
          action = Rand(1); 
	  assert((action >= 0 ) && (action <=1));
          char *page = (char*) (vmRegion + i * pageSize);
          
          snprintf(buffer, sizeof(buffer), format, id , i); 
          if(action == 0){ // <---- Write to page with what is in buffer
	        USLOSS_Console("Child (%d) writing to page %d\n", pid, i);
             if(k % 2 == 0){
                strcpy(page, buffer);
                valid[i] = 1;
             }
          } else if(action == 1){ // <---- Read from page and check that the content is correct  
             USLOSS_Console("Child (%d) reading from page %d\n", pid, i);
              if(valid[i]){ 
	            TEST(strcmp(page, buffer), 0);
              } else {
                 TEST(strcmp(page, empty), 0);
             }
          }
        }
    }
    PASSED();
    Debug("Child (%d) done.\n", pid);
    Sys_Terminate(42);
    return 0;
}


int
P4_Startup(void *arg)
{
    int     i;
    int     rc;
    int     pid;
    int     child;
    char   name[100];
    Debug("P4_Startup starting.\n");
    rc = Sys_VmInit(PAGES, PAGES, FRAMES, PAGERS, (void **) &vmRegion);
    TEST(rc, P1_SUCCESS);

    pageSize = USLOSS_MmuPageSize();

    for(i = 0; i < NUMCHILDREN; i++){
       snprintf(name, sizeof(name), "Child %d", i);
       rc = Sys_Spawn(name, Child, (void *) i, USLOSS_MIN_STACK * 2, 2, &pid);
       TEST(rc, P1_SUCCESS);
    }

    for (i = 0; i < NUMCHILDREN; i++) {
        rc = Sys_Wait(&pid, &child);
        TEST(rc, P1_SUCCESS);
        TEST(child, 42);
    }
    Sys_VmShutdown();
    Debug("P4_Startup done.\n");
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
