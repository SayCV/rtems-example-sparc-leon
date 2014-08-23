
/* Author: Magnus Hjorth, Aeroflex Gaisler AB
 * Contact: support@gaisler.com
 *
 * Revision history:
 *   2010-11-18, MH, First version 
 */

#include <string.h> /* For memset only */

#include "memscrub.h"

#ifndef NULL
#define NULL ((void *)0)
#endif

/* Register bits */
/* AHB status register */

#define CECOUNT       (1023<<22)
#define CECOUNT_SHIFT 22
#define UECOUNT       (255<<14)
#define UECOUNT_SHIFT 14
#define DONE          (1<<13)
#define SEC           (1<<11)
#define SBC           (1<<10)
#define CE            (1<<9)
#define NE            (1<<8)
#define NE_SHIFT      8
#define HWRITE        (1<<7)
#define HWRITE_SHIFT  7
#define HMASTER       (15<<3)
#define HMASTER_SHIFT 3
#define HSIZE         (7<<0)
#define HSIZE_SHIFT   0
/* AHB error config register */
#define CETHRES       (1023<<22)
#define CETHRES_SHIFT 22
#define UETHRES       (255<<14)
#define UETHRES_SHIFT 14
#define CECTE         2
#define UECTE         1
/* Scrubber status register */
#define RUNCOUNT_SHIFT 22
#define BURSTLEN       (15<<1)
#define BURSTLEN_SHIFT 1
#define ACTIVE         (1<<0)
/* Scrubber config register */
#define DELAY          (255<<8)
#define DELAY_SHIFT    8
#define IRQD           (1<<7)
#define SERA           (1<<5)
#define LOOP           (1<<4)
#define MODE           (3<<2)
#define MODE_SHIFT     2
#define MODE_SCRUB     (0<<MODE_SHIFT)
#define MODE_REGEN     (1<<MODE_SHIFT)
#define MODE_INIT      (2<<MODE_SHIFT)
#define ES             (1<<1)
#define SCEN           (1<<0)


static inline void waitstop(struct memscrub *s, unsigned long *scrubstat_val)
{
  unsigned long l = *scrubstat_val;
  while ((l & ACTIVE) != 0) {
    asm __volatile__ ("nop; nop; ");
    s->regs->scrubconf = 0;
    l = s->regs->scrubstat;
  }
  *scrubstat_val = l;
}

void memscrub_init(unsigned long regaddr, struct memscrub *s)
{
  unsigned long l;
  int i,j;
  memset(s,0,sizeof(*s));
  s->regs = (struct memscrub_regs *)regaddr;
  /* Disable error interrupts and stop scrubber */
  s->regs->ahberrconf = 0;
  s->regs->scrubconf = 0;
  /* Wait for scrubber to stop if it's running */
  l = s->regs->scrubstat;
  waitstop(s,&l);
  /* Detect block size in bytes and burst length */
  s->regs->scrubhighaddr = 0;
  s->blockmask = s->regs->scrubhighaddr;
  i = (l & BURSTLEN) >> BURSTLEN_SHIFT;
  for (j=1; i>0; i--) j <<= 1;
  s->burstlen = j;
}

/* Update lastat_ce/ue from status register and handle wrapping case */
static unsigned long checkstat(struct memscrub *s)
{
  unsigned long l;
  int i;
  l = s->regs->ahbstat;
  i = (l >> CECOUNT_SHIFT);
  if (i < s->laststat_ce) s->ceack += 1024;    
  s->laststat_ce = i;
  i = ((l & UECOUNT) >> UECOUNT_SHIFT);
  if (i < s->laststat_ue) s->ueack += 256;
  s->laststat_ue = i;
  return l;
}

static void clearstat(struct memscrub *s)
{
    /* Update internal counters */
    s->ceack += s->laststat_ce;
    s->ueack += s->laststat_ue;
    s->laststat_ce = s->laststat_ue = 0;
    /* Clear status reg. */
    s->regs->ahbstat = 0;
}

int memscrub_get_events(struct memscrub *s, 
			unsigned long *ahberr_addr, unsigned long *ahberr_data, 
			int *runcount)
{
  unsigned long l,x;
  int r=0;

  l = checkstat(s);
  
  /* Handle correctable/uncorrectable error */
  if ((l & NE) != 0) {
    *ahberr_addr = s->regs->ahbaddr;
    *ahberr_data = l & (HSIZE|HMASTER|HWRITE);
    if ((l & (CE | SBC | SEC)) != 0) {
      /* Correctable error threshold */
      r |= (l >> NE_SHIFT) & 14;
    } else {
      /* Uncorrectable error */
      r |= 1;
    }    
    /* Ack IRQ by clearing status reg. */
    clearstat(s);
  }

  /* Handle operation done */
  if ((l & DONE) != 0) {
    x = s->regs->scrubstat;
    *runcount = x >> RUNCOUNT_SHIFT;
    s->regs->scrubstat = 0;
    r |= 16;
  }
  
  return r;
}

void memscrub_operation_start(struct memscrub *s, 
			      unsigned long addr1, unsigned long size1, 
			      unsigned long addr2, unsigned long size2,
			      int delay, int mode, int flags)
{
  int i;
  unsigned long l;
  /* Make sure scrubber is stopped */
  s->regs->scrubconf = 0;
  l = s->regs->scrubstat;
  waitstop(s,&l);
  /* For clear: Fill FIFO with zeroes */
  if (mode == MODE_INIT)
    for (i=0; i<s->blockmask; i+=4)
      s->regs->scrubinit = 0;
  /* Set range */
  s->regs->scrublowaddr = addr1;
  s->regs->scrubhighaddr = addr1+size1-1;
  if ((flags & SERA) != 0) {
    s->regs->scrublowaddr2 = addr2;
    s->regs->scrubhighaddr2 = addr2+size2-1;
  }
  /* Set config register to start scrubber */
  l = (delay << DELAY_SHIFT) | mode | flags;
  s->regs->scrubconf = l;  
}

void memscrub_stop(struct memscrub *s)
{
  unsigned long l;
  s->regs->scrubconf = 0;
  l = s->regs->scrubstat;
  waitstop(s,&l);
}

void memscrub_disable(struct memscrub *s)
{
  s->regs->scrubconf = 0;
  s->regs->ahberrconf = 0;
  s->regs->ahbstat = 0;
}

void memscrub_switch_mode(struct memscrub *s, int mode, int new_delay, int new_flags)
{
  unsigned long l;
  l = s->regs->scrubconf;
  l &= ~MODE;
  l |= mode;
  if (new_delay >= 0) {
    l &= ~DELAY;
    l |= (new_delay << DELAY_SHIFT);
  }
  if (new_flags >= 0) {
    l &= ~(MEMSCRUB_FLAG_IRQD|MEMSCRUB_FLAG_LOOP|MEMSCRUB_FLAG_ES);
    l |= new_flags;
  }
  s->regs->scrubconf = l;
}

static unsigned long thresreg(int thres1, int thres2)
{
  unsigned long l;
  l = 0;
  if (thres1 >= 0)
    l |= 1 | (thres1 << 14);
  if (thres2 >= 0)
    l |= 2 | (thres2 << 22);
  return l;
}

void memscrub_setup_ahberr(struct memscrub *s, int uethres, int cethres)
{
  s->regs->ahberrconf = thresreg(uethres,cethres);
  if (uethres >= 0 || cethres >= 0) {
    checkstat(s);
    clearstat(s);
  }
}

void memscrub_setup_scruberr(struct memscrub *s, int blkthres, int runthres)
{
  s->regs->scruberrthres = thresreg(blkthres,runthres);
}

void memscrub_get_totals(struct memscrub *s, int totals[2])
{
  checkstat(s);
  totals[0] = s->ceack + s->laststat_ce;
  totals[1] = s->ueack + s->laststat_ue;  
}
