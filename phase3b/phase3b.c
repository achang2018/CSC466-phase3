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
    // allocate and initialize an empty page table
    return table;
}
