/*
 * phase3b.c
 *
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
    int offset = (int)(*arg);
    int pid = P1_GetPid();
    int pageSize = USLOSS_MmuPageSize();
    int page = offset/pageSize;

    if (type == USLOSS_MMU_FAULT) {
      USLOSS_Console("PAGE FAULT!!! PID %d page %d\n", pid, page);
      USLOSS_PTE* pageTable;
      int ret = P3PageTableGet(pid, &pageTable);
      if (ret != P1_SUCCESS || pageTable == NULL) {
          USLOSS_Console("PAGE FAULT!!! PID %d has no page table!!!\n", pid);
          USLOSS_Halt(1);
      } else {
        // determine which page suffered the fault (USLOSS_MmuPageSize)
        //     update the page's PTE to map page x to frame x
        //     set the PTE to be read-write and incore
        //     update the page table in the MMU (USLOSS_MmuSetPageTable)
      }
    } else {
      if (type == USLOSS_MMU_ACCESS) {
        USLOSS_Console("PROTECTION VIOLATION!!! PID %d offset 0x%x!!!\n", pid, offset);
      } else {
        USLOSS_Console("Unknown cause: %d\n", cause);
      }
      USLOSS_Halt(1);
    }

    /*******************
    get the offset from *arg
    get pid from P1_GetPid()
    get pageSize from USLOSS_MmuPageSize()
    calculate page from offset/pageSize

    if the cause is USLOSS_MMU_FAULT (USLOSS_MmuGetCause)
        print fault message e.g.,: 
          USLOSS_Console("PAGE FAULT!!! PID %d page %d\n", pid, page);
        if the process does not have a page table  (P3PageTableGet)
            print error message e.g., :
              USLOSS_Console("PAGE FAULT!!! PID %d has no page table!!!\n", pid);
            USLOSS_Halt(1)
        else
            determine which page suffered the fault (USLOSS_MmuPageSize)
            update the page's PTE to map page x to frame x
            set the PTE to be read-write and incore
            update the page table in the MMU (USLOSS_MmuSetPageTable)
    else
        if the casue is USLOSS_MMU_ACCESS
            print error message:
              USLOSS_Console("PROTECTION VIOLATION!!! PID %d offset 0x%x!!!\n", pid, offset);
        else
            print error message:
              USLOSS_Console("Unknown cause: %d\n", cause);
        USLOSS_Halt(1)
    *********************/


}

USLOSS_PTE *
P3PageTableAllocateEmpty(int pages)
{
    USLOSS_PTE  *table = NULL;
    USLOSS_PTE *tail = table;
    int i=0;
    while (i < pages) {
      USLOSS_PTE newEntry = malloc(sizeof(USLOSS_PTE));
      newEntry.incore = 0;
      *tail = newEntry;
      tail++;
    }

    // allocate and initialize an empty page table
    return table;
}
