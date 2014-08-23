
/* Generic C "driver" for the memscrub memory scrubber and status register
 * Author: Magnus Hjorth, Aeroflex Gaisler
 * Contact: support@gaisler.com */

#ifndef MEMSCRUB_H_INCLUDED
#define MEMSCRUB_H_INCLUDED

/* Register structure */
struct memscrub_regs {
  volatile unsigned long ahbstat;
  volatile unsigned long ahbaddr;
  volatile unsigned long ahberrconf;
  volatile unsigned long res;
  volatile unsigned long scrubstat;
  volatile unsigned long scrubconf;
  volatile unsigned long scrublowaddr;
  volatile unsigned long scrubhighaddr;
  volatile unsigned long scrubpos;
  volatile unsigned long scruberrthres;
  volatile unsigned long scrubinit;
  volatile unsigned long scrublowaddr2;
  volatile unsigned long scrubhighaddr2;
};

struct memscrub;

/* Driver state structure */
struct memscrub {
  struct memscrub_regs *regs;
  unsigned long blockmask;
  int burstlen;
  int ceack,ueack;
  int laststat_ce,laststat_ue;
};

/* Initialize driver - call this first
 * regaddr: Address of memscrub registers 
 * s: State structure filled in by function */

void memscrub_init(unsigned long regaddr, struct memscrub *s);

/* Set the different error thresholds. Negative argument means disabling the error threshold */
void memscrub_setup_ahberr(struct memscrub *s, int uethres, int cethres);
void memscrub_setup_scruberr(struct memscrub *s, int blkthres, int runthres);

/* Quicker macros equivalent to calling memscrub_setup_ahberr(s,-1,-1) */
#define memscrub_disable_ahberr(s) ((s)->regs->ahberrconf = 0)
#define memscrub_disable_scruberr(s) ((s)->regs->scruberrthres = 0)


/* Event constants used for return value of memscrub_get_events
 *   MEMSCRUB_UE        (1) - Uncorrectable error count threshold exceeded
 *   MEMSCRUB_CE        (2) - Correctable error count threshold exceeded
 *   MEMSCRUB_CE_RUNBLK (4) - Correctable error count during scrub run exceeded threshold
 *   MEMSCRUB_CE_RUNTOT (8) - Correctable error during single scrub block exceeded threshold
 *   MEMSCRUB_DONE     (16) - Operation done
 */
#define MEMSCRUB_UE        1
#define MEMSCRUB_CE        2
#define MEMSCRUB_CE_RUNBLK 4
#define MEMSCRUB_CE_RUNTOT 8
#define MEMSCRUB_DONE      16

/* Check the status registers for events (AHB errors,operation done)
 * Returns: Mask of output events 
 *
 * ahberr_addr: In case of UE/CE/SBC/SEC, address of failed access 
 *
 * ahberr_data: In case of UE/CE/SBC/SEC, other parameters (master,hsize,hwrite)
 *
 * runcount: In case of DONE after a single scrub run, number of CE:s
 * that occurred during the iteration. This value should not be used
 * when using loop mode, because the run count field is cleared
 * immediately in hardware when the next loop iteration starts.
 */
int memscrub_get_events(struct memscrub *s, 
			unsigned long *ahberr_addr, unsigned long *ahberr_data,
			int *runcount);

/* Stop the current operation */
void memscrub_stop(struct memscrub *s);

/* Disables interrupts and commands scrubber to stop. */
void memscrub_disable(struct memscrub *s);

/* Switch mode (scrub->regen normally). If new_delay<0, the current delay value is kept. */
void memscrub_switch_mode(struct memscrub *s, int new_mode, int new_delay, int new_flags);

/* Read out the current value of the scrub iteration error counter */
#define memscrub_get_runcount(s) ((s)->regs->scrubstat >> 22)

/* Read out the total number of correctable/uncorrectable errors encountered */
void memscrub_get_totals(struct memscrub *s, int totals[2]);



/*************************************************************************
 * The procedures/macros below start different scrubber operations. Each
 * macro has two versions, one single-range version and one
 * dual-range version.
 *
 * If another operation is already in progress, the code will stop the
 * current operation before proceeding. 
 *
 * You can supply a callback that is called (from memscrub_work) when
 * the operation is completed. If NULL is given as callback pointer,
 * no function gets called. 
 *
 * The delay parameter controls the number of cycles between each 
 * burst. To convert this into speed, you need to know the access time
 * of the memory slave and the burst length. 
 ***********************************************************************
 */

/* Generic "Expert" interface used by the macros below */
void memscrub_operation_start(struct memscrub *s, 
			      unsigned long addr1, unsigned long size1, 
			      unsigned long addr2, unsigned long size2,
			      int delay, int mode, int flags);


#define MEMSCRUB_FLAG_IRQD    (1<<7)
#define MEMSCRUB_FLAG_SERA    (1<<5)
#define MEMSCRUB_FLAG_LOOP    (1<<4)
#define MEMSCRUB_MODE_SCRUB   (0<<2)
#define MEMSCRUB_MODE_REGEN   (1<<2)
#define MEMSCRUB_MODE_INIT    (2<<2)
#define MEMSCRUB_FLAG_ES      (1<<1)
#define MEMSCRUB_FLAG_SCEN    (1<<0)

/* These procedure prototypes are implemented as macros below.

Start clearing a memory range using the scrubber.
void memscrub_clear(struct memscrub *s, unsigned long addr, unsigned long size,
		    int delay, memscrub_done_cb done_cb);

Start read-writing a memory range using the scrubber. 
void memscrub_regen(struct memscrub *s, unsigned long addr, unsigned long size,
		    int delay, memscrub_done_cb done_cb);

Start scrubbing a memory range once using the scrubber. 
void memscrub_scrub(struct memscrub *s, unsigned long addr, unsigned long size,
		    int delay, memscrub_done_cb done_cb);

Set up the scrubber to periodically scrub an area 
void memscrub_scrub_loop(struct memscrub *s, 
			 unsigned long addr, unsigned long size,
			 int delay, memscrub_done_cb done_cb);

Set up the scrubber to periodically scrub an area based on the 
external trigger (timer) input 
void memscrub_scrub_trig(struct memscrub *s, 
			 unsigned long addr, unsigned long size,
			 int delay, memscrub_done_cb done_cb);
*/

#define memscrub_clear(s,addr,size,delay,flags)				\
  memscrub_operation_start(s,addr,size,0,0,delay,MEMSCRUB_MODE_INIT,MEMSCRUB_FLAG_SCEN|(flags))
#define memscrub_regen(s,addr,size,delay,flags)				\
  memscrub_operation_start(s,addr,size,0,0,delay,MEMSCRUB_MODE_REGEN,MEMSCRUB_FLAG_SCEN|(flags))
#define memscrub_scrub(s,addr,size,delay,flags)				\
  memscrub_operation_start(s,addr,size,0,0,delay,MEMSCRUB_MODE_SCRUB,MEMSCRUB_FLAG_SCEN|(flags))
#define memscrub_scrub_loop(s,addr,size,delay,flags)			\
  memscrub_operation_start(s,addr,size,0,0,delay,MEMSCRUB_MODE_SCRUB,MEMSCRUB_FLAG_SCEN|MEMSCRUB_FLAG_LOOP|(flags))
#define memscrub_scrub_trig(s,addr,size,delay,flags)			\
  memscrub_operation_start(s,addr,size,0,0,delay,MEMSCRUB_MODE_SCRUB,MEMSCRUB_FLAG_ES|(flags))
#define memscrub_clear2(s,addr1,size1,addr2,size2,delay,flags)		\
  memscrub_operation_start(s,addr1,size1,addr2,size2,delay,MEMSCRUB_MODE_INIT,MEMSCRUB_FLAG_SCEN|MEMSCRUB_FLAG_SERA|(flags));
#define memscrub_regen2(s,addr1,size1,addr2,size2,delay,flags)		\
  memscrub_operation_start(s,addr1,size1,addr2,size2,delay,MEMSCRUB_MODE_REGEN,MEMSCRUB_FLAG_SCEN|MEMSCRUB_FLAG_SERA|(flags));
#define memscrub_scrub2(s,addr1,size1,addr2,size2,delay,flags)		\
  memscrub_operation_start(s,addr1,size1,addr2,size2,delay,MEMSCRUB_MODE_SCRUB,MEMSCRUB_FLAG_SCEN|MEMSCRUB_FLAG_SERA|(flags));
#define memscrub_scrub_loop2(s,addr1,size1,addr2,size2,delay,flags)	\
  memscrub_operation_start(s,addr1,size1,addr2,size2,delay,MEMSCRUB_MODE_SCRUB,MEMSCRUB_FLAG_SCEN|MEMSCRUB_FLAG_LOOP|MEMSCRUB_FLAG_SERA|(flags));
#define memscrub_scrub_trig2(s,addr1,size1,addr2,size2,delay,flags)		\
  memscrub_operation_start(s,addr1,size1,addr2,size2,delay,MEMSCRUB_MODE_SCRUB,MEMSCRUB_FLAG_ES|MEMSCRUB_FLAG_SERA|(flags));

#endif
