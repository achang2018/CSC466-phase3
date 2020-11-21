/**
 *  Authors: Bianca Lara, Ann Chang
 *  Due Date: November 9th, 2020
 *  Phase 2b
 *  Submission Type: Group
 *  Comments: The phase 2b implementation of phase 2. Implements the 
 *  allocation of the page table and the page fault handler for
 *  dynamic page allocation. The page fault handler updates the page
 *  table to bring in the missing frame with incore and read-write set
 */

#include <assert.h>
#include <phase1.h>
#include <phase2.h>
#include <usloss.h>
#include <string.h>
#include <libuser.h>

#include "phase3Int.h"

void
P3PageFaultHandler(int type, void *arg)
{
    int offset = (int) arg;               // get the offset from *arg
    int pid = P1_GetPid();                // get pid from P1_GetPid()
    int pageSize = USLOSS_MmuPageSize();  // get pageSize from USLOSS_MmuPageSize()
    int page = offset/pageSize;           // calculate page from offset/pageSize
    int cause = USLOSS_MmuGetCause();     // get cause from USLOSS_MmuGetCause()

    // If the cause is USLOSS_MMU_FAULT
    if (cause == USLOSS_MMU_FAULT) {
      USLOSS_Console("PAGE FAULT!!! PID %d page %d\n", pid, page);
      
      // Determine if the process has a page table
      USLOSS_PTE* pageTable;
      int ret = P3PageTableGet(pid, &pageTable);
      if (ret != P1_SUCCESS || pageTable == NULL) {
          USLOSS_Console("%d\n", ret);
          USLOSS_Console("PAGE FAULT!!! PID %d has no page table!!!\n", pid);
          USLOSS_Halt(1);
      } else {
        // If the process has a page table, update page table to map
        // page x to frame x, set read-write and incore
        pageTable[page].frame = page;
        pageTable[page].incore = 1;
        pageTable[page].read = 1;
        pageTable[page].write = 1;
        // Update the page table in the MMU
        int ret = USLOSS_MmuSetPageTable(pageTable);
        assert(ret == USLOSS_MMU_OK);
      }
    } else {
      // If the cuase is USLOSS_MMU_ACCESS
      if (cause == USLOSS_MMU_ACCESS) {
        USLOSS_Console("PROTECTION VIOLATION!!! PID %d offset 0x%x!!!\n", pid, offset);
      } else {
        // If the cause is unknown
        USLOSS_Console("Unknown cause: %d\n", cause);
      }
      USLOSS_Halt(1);
    }
}

USLOSS_PTE *
P3PageTableAllocateEmpty(int pages)
{
    // allocate and initialize an empty page table
    USLOSS_PTE  *table = NULL;
    if (pages > 0) {
      // Initialize the first page in the array
      table = malloc(sizeof(USLOSS_PTE));
      table->incore = 0;
      USLOSS_PTE *tail = table;
      tail++;
      // Initialize the rest of the pages if applicable
      int i=1;
      while (i < pages) {
        tail = malloc(sizeof(USLOSS_PTE));
        tail->incore = 0;
        tail++;
        i++;
      }
    }
    return table;
}
