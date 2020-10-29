/*
 * Internal Phase 2 definitions. These may be used by other functions in Phase 2.
 * Version 1.0
 * DO NOT MODIFY THIS FILE.
 */

#ifndef _PHASE2_INT_H
#define _PHASE2_INT_H

#include "phase2.h"

// Phase 2a

void    P2ProcInit(void);

// Phase 2b

void    P2ClockInit(void);
void    P2ClockShutdown(void);

// Phase 2c

void    P2DiskInit(void);
void    P2DiskShutdown(void);

#endif