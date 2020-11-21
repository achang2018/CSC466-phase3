/*
 * test_stress.c
 *  
 *  Stress test case for Phase 3a. It creates 10 processes, Child. 
 *  Each Child writes to its pages and reads what was written at random.
 *
 *  Each process has four pages and there are four frames. Phase 3a implements identity page
 *  tables so that page x -> frame x for all processes. 
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


#define DEBUG 1
#define PAGES 4        
#define ITERATIONS 100
#define NUMCHILDREN 10

#define Rand(limit) ((int) ((((double)((limit)+1)) * rand()) / \
            ((double) RAND_MAX)))

typedef int PID;
typedef int SID;

static char *vmRegion;
static int  pageSize;

#ifdef DEBUG
int debugging = 1;
#else
int debugging = 0;
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
    PID     pid;
    char    buffer[128];
    char    empty[128] = "";
    int     action;
    int     valid[PAGES];
    Sys_GetPID(&pid);
    Debug("Child (%d) starting.\n", pid);

    for(int j =0; j < PAGES; j++){
       char *page = (char *) (vmRegion + j * pageSize);
       strcpy(page, empty); 	
       valid[j] = 0;
    }

    for(int k = 0; k < ITERATIONS; k++){ 
       for (int i = 0; i < PAGES; i++) {
          action = Rand(1); 
	  assert((action >= 0 ) && (action <=1));
          char *page = (char*) (vmRegion + i * pageSize);
          
          snprintf(buffer, sizeof(buffer), format, id , i); 
          if(action == 0){ // <---- Write to page with what is in buffer
	    // USLOSS_Console("Child (%d) writing to page %d\n", pid, i);
             if(k % 2 == 0){
                strcpy(page, buffer);
                valid[i] = 1;
             }
          } else if(action == 1){ // <---- Read from page and check that the content is correct  
             // USLOSS_Console("Child (%d) reading from page %d\n", pid, i);
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
    PID     pid;
    PID     child;
    char   name[100];
    Debug("P4_Startup starting.\n");
    rc = Sys_VmInit(PAGES, PAGES, PAGES, 0, (void **) &vmRegion);
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
