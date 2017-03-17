#include "hilevel.h"

extern void     main_console();
extern uint32_t tos_console;
extern void     main_P3();
extern uint32_t tos_P3;
extern void     main_P4();
extern uint32_t tos_P4;
extern void     main_P5();
extern uint32_t tos_P5;

pcb_t pcb[ MAX_PROG ], *current = NULL;
pid_t topID = 0;

pid_t findProgIndex(int id){
  for(int i=0;i<MAX_PROG;i++){
    if(pcb[i].pid == i){
      return i;
    }
  }
  return -1;
}

void scheduler( ctx_t* ctx ) {
  /*if      ( current == &pcb[ 0 ] ) {
    memcpy( &pcb[ 0 ].ctx, ctx, sizeof( ctx_t ) );
    memcpy( ctx, &pcb[ 1 ].ctx, sizeof( ctx_t ) );
    current = &pcb[ 1 ];
  }
  else if ( current == &pcb[ 1 ] ) {
    memcpy( &pcb[ 1 ].ctx, ctx, sizeof( ctx_t ) );
    memcpy( ctx, &pcb[ 2 ].ctx, sizeof( ctx_t ) );
    current = &pcb[ 2 ];
  }
  else if ( current == &pcb[ 2 ] ) {
    memcpy( &pcb[ 2 ].ctx, ctx, sizeof( ctx_t ) );
    memcpy( ctx, &pcb[ 0 ].ctx, sizeof( ctx_t ) );
    current = &pcb[ 0 ];
  }*/

  pid_t ind = findProgIndex(current->pid);
  pid_t nextInd = findNextPcbInd(ind);
  if(nextInd >= 0){
    memcpy(&pcb[ ind ].ctx,ctx,sizeof( ctx_t ));
    memcpy(ctx, &pcb[ nextInd ].ctx,sizeof( ctx_t ));
    current = &pcb[ nextInd ];
  }
  return;
}

void hilevel_handler_rst(ctx_t* ctx) {
  TIMER0->Timer1Load  = 0x00100000; // select period = 2^20 ticks ~= 1 sec
  TIMER0->Timer1Ctrl  = 0x00000002; // select 32-bit   timer
  TIMER0->Timer1Ctrl |= 0x00000040; // select periodic timer
  TIMER0->Timer1Ctrl |= 0x00000020; // enable          timer interrupt
  TIMER0->Timer1Ctrl |= 0x00000080; // enable          timer

  GICC0->PMR          = 0x000000F0; // unmask all            interrupts
  GICD0->ISENABLER1  |= 0x00000010; // enable timer          interrupt
  GICC0->CTLR         = 0x00000001; // enable GIC interface
  GICD0->CTLR         = 0x00000001; // enable GIC distributor

  pcb[ 0 ].pid      = 1;
  pcb[ 0 ].ctx.cpsr = 0x50;
  pcb[ 0 ].ctx.pc   = ( uint32_t )( &main_console );
  pcb[ 0 ].ctx.sp   = ( uint32_t )( &tos_console  );

  for(int i = 1;i<MAX_PROG;i++){
    memset( &pcb[ i ], 0, sizeof( pcb_t ) );
  }
  int_enable_irq();

  /*memset( &pcb[ topID ], 0, sizeof( pcb_t ) );
  pcb[ topID ].pid      = topID++;
  pcb[ topID ].ctx.cpsr = 0x50;
  pcb[ topID ].ctx.pc   = ( uint32_t )( &main_console );
  pcb[ topID ].ctx.sp   = ( uint32_t )( &tos_console  );
  topID+=1;*/


  /*memset( &pcb[ topID ], 0, sizeof( pcb_t ) );
  pcb[ topID ].pid      = topID++;
  pcb[ topID ].ctx.cpsr = 0x50;
  pcb[ topID ].ctx.pc   = ( uint32_t )( &main_P3 );
  pcb[ topID ].ctx.sp   = ( uint32_t )( &tos_P3  );
  topID+=1;

  memset( &pcb[ topID ], 0, sizeof( pcb_t ) );
  pcb[ topID ].pid      = topID++;
  pcb[ topID ].ctx.cpsr = 0x50;
  pcb[ topID ].ctx.pc   = ( uint32_t )( &main_P4 );
  pcb[ topID ].ctx.sp   = ( uint32_t )( &tos_P4  );
  topID+=1;

  memset( &pcb[ topID ], 0, sizeof( pcb_t ) );
  pcb[ topID ].pid      = topID++;
  pcb[ topID ].ctx.cpsr = 0x50;
  pcb[ topID ].ctx.pc   = ( uint32_t )( &main_P5 );
  pcb[ topID ].ctx.sp   = ( uint32_t )( &tos_P5  );
  topID+=1;

  /* Once the PCBs are initialised, we (arbitrarily) select one to be
   * restored (i.e., executed) when the function then returns.
   */

  current = &pcb[ 0 ];
  memcpy( ctx, &current->ctx, sizeof( ctx_t ) );
  return;
}

void hilevel_handler_svc( ctx_t* ctx, uint32_t id ) {
  /* Based on the identified encoded as an immediate operand in the
   * instruction,
   *
   * - read  the arguments from preserved usr mode registers,
   * - perform whatever is appropriate for this system call,
   * - write any return value back to preserved usr mode registers.
   */

  switch( id ) {
    case 0x00 : { // 0x00 => yield()
      scheduler( ctx );
      break;
    }
    case 0x01 : { // 0x01 => write( fd, x, n )
      int   fd = ( int   )( ctx->gpr[ 0 ] );
      char*  x = ( char* )( ctx->gpr[ 1 ] );
      int    n = ( int   )( ctx->gpr[ 2 ] );

      for( int i = 0; i < n; i++ ) {
        PL011_putc( UART0, *x++, true );
      }

      ctx->gpr[ 0 ] = n;
      break;
    }
    case 0x03 : { // 0x03 => fork()
      uint32_t childID = hilevel_fork(ctx);
      ctx->gpr[ 0 ] = childID;
      break;
    }
    case 0x05 : {
      hilevel_exec(ctx);
      break;
    }
    default   : { // 0x?? => unknown/unsupported
      break;
    }
  }

  return;
}

void hilevel_handler_irq(ctx_t* ctx) {
  // Step 2: read  the interrupt identifier so we know the source.

  uint32_t id = GICC0->IAR;

  // Step 4: handle the interrupt, then clear (or reset) the source.

  if( id == GIC_SOURCE_TIMER0 ) {
    //scheduler(ctx);
    TIMER0->Timer1IntClr = 0x01;

  }

  // Step 5: write the interrupt identifier to signal we're done.

  GICC0->EOIR = id;

  return;
}

int findFreeSlot(){
  for(int i=0;i<MAX_PROG;i++){
    if(pcb[i].pid == 0 || pcb[i].status == TERMINATED){
      return i;
    }
  }
  return -1;
}

int findNextPcbInd(int ind){
  for(int i=1;i<MAX_PROG+1;i++){
    if(pcb[(ind+i)%MAX_PROG].pid != 0 && pcb[(ind+i)%MAX_PROG].status != TERMINATED){
      return (ind+i)%MAX_PROG;
    }
  }
  return -1;
}

uint32_t hilevel_fork(ctx_t* ctx){
  int newInd = findFreeSlot();
  if(newInd>=0){
    memset( &pcb[ newInd ], 0, sizeof( pcb_t ) );
    pcb[ newInd ].pid = topID+1;

    uint32_t new_tos = tos_console + STACK_CAP * newInd;
    uint32_t curr_tos = tos_console + STACK_CAP * findProgIndex(current->pid);
    uint32_t stackSize = curr_tos - ctx->sp;
    uint32_t newSP = new_tos - stackSize;
    memcpy(newSP, curr_tos - stackSize, stackSize);

    memcpy( &pcb[ newInd ].ctx, ctx, sizeof( ctx_t) );
    pcb[ newInd ].ctx.gpr[0] = 0;
    topID+=1;
    current = &pcb[newInd];
  }
  return pcb[ newInd ].pid;
}

void hilevel_exec(ctx_t* ctx){
  cPrint("Break\n",6);
  uint32_t curr_tos = tos_console + STACK_CAP * findProgIndex(current->pid);
  memset(curr_tos - STACK_CAP, 0, STACK_CAP);
  ctx->sp = curr_tos;
  ctx->pc = ctx->gpr[ 0 ];
}

void cPrint(char* x, int n){
  for( int i = 0; i < n; i++ ) {
    PL011_putc( UART0, *x++, true );
  }
}
