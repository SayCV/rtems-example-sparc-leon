/* Sample bdinit routine to clear RAM on power-up using the scrubber */

#include "memscrub.h"

void bdinit1(void)
{
  struct memscrub_regs * const scrubregs = (struct memscrub_regs *)(MEMSCRUB_BASE);
  int i;
  scrubregs->scrublowaddr = (MEMSTART);
  scrubregs->scrubhighaddr = (MEMSTART)+(MEMSIZE)-1;
  for (i=0; i<8; i++)
    scrubregs->scrubinit = 0x10203040;
  scrubregs->scrubconf = 1;
  while ((scrubregs->scrubstat & 1) != 0) { asm __volatile__ ("nop; nop;\n"); }
}

void bdinit2(void)
{
}
