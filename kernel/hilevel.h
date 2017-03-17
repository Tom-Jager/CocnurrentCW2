#ifndef __HILEVEL_H
#define __HILEVEL_H
#define MAX_PROG  20
#define STACK_CAP 0x00001000

// Include functionality relating to newlib (the standard C library).

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <string.h>

// Include functionality relating to the platform.

#include   "GIC.h"
#include "PL011.h"
#include "SP804.h"

// Include functionality relating to the   kernel.

#include "lolevel.h"
#include     "int.h"

/* The kernel source code is made simpler via three type definitions:
 *
 * - a type that captures a Process IDentifier (PID), which is really
 *   just an integer,
 * - a type that captures each component of an execution context (i.e.,
 *   processor state) in a compatible order wrt. the low-level handler
 *   preservation and restoration prologue and epilogue, and
 * - a type that captures a process PCB.
 */

typedef int pid_t;

typedef enum {
  CREATED,
  READY,
  WAITING,
  EXECUTING,
  TERMINATED
} status_t;

typedef struct {
  uint32_t cpsr, pc, gpr[ 13 ], sp, lr;
} ctx_t;

typedef struct {
  pid_t pid;
  ctx_t ctx;
  status_t status;
} pcb_t;

pid_t findProgIndex(int id);

void scheduler(ctx_t* ctx);

void hilevel_handler_rst(ctx_t* ctx);

void hilevel_handler_svc( ctx_t* ctx, uint32_t id );

void hilevel_handler_irq(ctx_t* ctx);

int findNextFreeSlot(int ind);

uint32_t hilevel_fork(ctx_t* ctx);

void hilevel_exec(ctx_t* ctx);

void cPrint(char* x, int n);


#endif
