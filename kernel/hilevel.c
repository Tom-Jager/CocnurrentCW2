#include "hilevel.h"

extern void     main_console();
extern uint32_t tos_console;
extern void     main_P3();
extern uint32_t tos_P3;
extern void     main_P4();
extern uint32_t tos_P4;
extern void     main_P5();
extern uint32_t tos_P5;

pcb_t pcb[ 4 ], *current = NULL;
pid_t topID = 0;

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

  pid_t currID = current->pid;
  memcpy(&pcb[ currID-1 ].ctx,ctx,sizeof( ctx_t ));
  memcpy(ctx, &pcb[ currID % topID ].ctx,sizeof( ctx_t ));
  current = &pcb[ currID % topID ];

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

  int_enable_irq();

  /*memset( &pcb[ topID ], 0, sizeof( pcb_t ) );
  pcb[ topID ].pid      = topID++;
  pcb[ topID ].ctx.cpsr = 0x50;
  pcb[ topID ].ctx.pc   = ( uint32_t )( &main_console );
  pcb[ topID ].ctx.sp   = ( uint32_t )( &tos_console  );
  topID+=1;*/
  pcb[ 0 ].pid      = 1;
  pcb[ 0 ].ctx.cpsr = 0x50;
  pcb[ 0 ].ctx.pc   = ( uint32_t )( &main_console );
  pcb[ 0 ].ctx.sp   = ( uint32_t )( &tos_console  );

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
      memset( &pcb[ topID ], 0, sizeof( pcb_t ) );
      pcb[ topID ].pid      = topID++;
      memcpy( &pcb[ topID ].ctx, &current->ctx, sizeof( ctx_t) );
      topID+=1;

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
