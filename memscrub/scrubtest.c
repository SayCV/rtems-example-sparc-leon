
/* Application example for using the DDR2SPA with EDAC together with
 * the memory scrubber under the Gaisler RTEMS environment.
 * 
 * Author: Magnus Hjorth, Aeroflex Gaisler
 * Contact: support@gaisler.com
 */

#include <stdio.h>
#include <stdlib.h>
#include <rtems.h>
#include <amba.h>
#include "memscrub_rtems.h"

/* -------------- Application setup ----------   */
#ifndef MEMSTART
#define MEMSTART (0x40000000)
#endif
#ifndef MEMSIZE
#define MEMSIZE  (0x1000000)
#endif
#ifndef OPERMODE
#define OPERMODE 0
#endif

/* ---------------- RTEMS Setup ---------------  */

/* configuration information */
#define CONFIGURE_INIT
#include <bsp.h> /* for device driver prototypes */

rtems_task Init( rtems_task_argument argument);	/* forward declaration needed */

/* configuration information */
#define CONFIGURE_APPLICATION_NEEDS_CONSOLE_DRIVER
#define CONFIGURE_APPLICATION_NEEDS_CLOCK_DRIVER
#define CONFIGURE_MAXIMUM_TASKS             4
#define CONFIGURE_MAXIMUM_MESSAGE_QUEUES    MEMSCRUB_MAXIMUM_MESSAGE_QUEUES
#define CONFIGURE_MESSAGE_BUFFER_MEMORY     MEMSCRUB_MESSAGE_BUFFER_MEMORY
#define CONFIGURE_RTEMS_INIT_TASKS_TABLE
#define CONFIGURE_EXTRA_TASK_STACKS         (3 * RTEMS_MINIMUM_STACK_SIZE)

#include <rtems/confdefs.h>

#define CONFIGURE_DRIVER_AMBAPP_GAISLER_GPTIMER
#define CONFIGURE_DRIVER_AMBAPP_GAISLER_APBUART
#define CONFIGURE_DRIVER_CUSTOM1
#define DRIVER_CUSTOM1_REG { memscrubr_register }

#include <drvmgr/drvmgr_confdefs.h>

static struct rtems_drvmgr_key memscrub_res[] = {
  { "memstart",  KEY_TYPE_INT, { .i = (MEMSTART) } }, 
  { "memsize",   KEY_TYPE_INT, { .i = (MEMSIZE) } },
  { "autostart", KEY_TYPE_INT, { .i = 1 } },
  { "opermode",  KEY_TYPE_INT, { .i = (OPERMODE) } },
  KEY_EMPTY
};

struct rtems_drvmgr_drv_res grlib_drv_resources[] = {
  { DRIVER_AMBAPP_GAISLER_MEMSCRUB_ID, 0, memscrub_res },
  RES_EMPTY
};


/* ----------------- Application code ---------------- */

struct ddr2info {
  volatile unsigned long *regs;
  unsigned long start,size;
};



/* /////////
 * Report task. Reads the scrubber message queue and prints a text
 * message whenever something has happened. */

static rtems_task report_task_entry(rtems_task_argument ignored)
{
  int i;
  struct memscrubr_message m;
  while (1) {
    i = memscrubr_get_message(0,1,&m);
    switch (m.msgtype) {
    case 1: 
      printf("[R] Scrubber iteration done, errcount=%d\n",m.d.done.cecount);
      break;
    case 2:
      printf("[R] %s detected, addr=%08x, %s, mst=%d, size=%d\n",
	     (m.d.err.errtype==2)?"CE":"UE",
	     (unsigned int)m.d.err.addr, m.d.err.hwrite?"wr":"rd",
	     m.d.err.master, (1<<m.d.err.hsize));
      break;
    case 3:
      printf("[R] Scrubber switched to regeneration mode\n");
      break;
    default:
      printf("[R] memscrubr_get_message returned %d, msgtype %d\n",i,m.msgtype);
      break;
    }
  }
}



/* //////// 
 * Fault injection task.  Injects single errors using the DDR2 FT
 * diagnostic registers. The errors are injected in 10 second intervals, the
 * first time 1 error is injected, then 2 errors are injected, then 3,
 * ..., up to 20, it then starts over at 1
 */


static int injected_total = 0;

static void flip_bit(volatile unsigned long *v, int bit)
{
  /* This should ideally be done with an atomic access such as a
   * compare-and-swap instruction in a loop so that the change is
   * completely atomic. With this implementation there is a tiny risk
   * that the memory is changed between read/write, for example if the
   * memory happens to be in the stack area and an interrupt is
   * triggered */
  (*v) ^= (1<<bit);
}

static rtems_task fault_task_entry(rtems_task_argument arg)
{
  int i,j,y;
  rtems_interval t,d;
  struct ddr2info *di = (struct ddr2info *)arg;
  unsigned long x;
  int totals[2];
  t = rtems_clock_get_ticks_since_boot();
  while (1) {
    for (i=1; i<=20; i++) {
      
      /* Wake up next multiple of 10 s */
      t += rtems_clock_get_ticks_per_second() * 10;
      d = t - rtems_clock_get_ticks_since_boot();
      rtems_task_wake_after(d);

      /* Inject errors */
      memscrubr_get_totals(0,totals);
      printf("[F] Counts: total_inj=%d, total_ce=%d\n"
	     "[F] --> Injecting %d errors <-- \n",injected_total,totals[0],i);

      

      for (j=0; j<i; j++) {
	/* Select random address to put in diag address register */
	x = (rand() % MEMSIZE) + MEMSTART;
	di->regs[9] = x;
	/* Select one of the 32 bits in either the diag data register (0-31) 
	 * or the diag CB register (32-63) */
	y = rand() % 64;
	if (y > 31)
	  flip_bit(di->regs+10, y-32);
	else
	  flip_bit(di->regs+11, y);
      }
      injected_total += i;
    }
  }
}



/* /////////
 * Init task. */

static struct ddr2info di;

static int ddr2_scan_func(struct ambapp_dev *dev, int index, int maxdepth, void *arg)
{
  struct ddr2info *info = (struct ddr2info *)arg;
  struct ambapp_ahb_info *ai = (struct ambapp_ahb_info *)(&(dev->devinfo));
  int i,memfound=0,regfound=0;

  for (i=0; i<4; i++) {
    if (ai->type[i] == AMBA_TYPE_MEM && !memfound) {
      info->start = ai->start[i];
      info->size = ( ~ (ai->mask[i]) )+1;
      memfound++;
    } else if (ai->type[i] == AMBA_TYPE_AHBIO && !regfound) {
      info->regs = (volatile unsigned long *)ai->start[i];
      regfound++;
    }
  }
  
  if (!memfound || !regfound) return 0;
  
  return 1;  
}

rtems_task Init(
  rtems_task_argument ignored
)
{
  int i;
  rtems_id report_task,fault_task;

  printf("-- Scrubber RTEMS test application --\n");  
  printf("Config: Mem range: %08x-%08x\n", (unsigned int)MEMSTART, 
	 (unsigned int)(MEMSTART+MEMSIZE-1));
  /* Check for scrubber */
  if (memscrubr_count() < 1) {
    printf("Error: No memscrub devices found!\n");
    exit(1);
  }
  memscrubr_print_status(0);
  /* Find DDR2 controller */
  i = ambapp_for_each(ambapp_plb.root,OPTIONS_ALL|OPTIONS_ALL_DEVS,
		      VENDOR_GAISLER,GAISLER_DDR2SP,10,ddr2_scan_func,
		      (void *)&di);
  if (i != 1) {
    printf("Error: DDR2 controller not found!\n");
    exit(1);
  }
  if ((di.regs[1] & (1<<16)) == 0) {
    printf("Error: DDR2 controller does not have EDAC!\n");
    exit(1);
  }
  printf("FT DDR2 controller found, Memory bar %d MB @ %08x, regs @ %08x\n\n",
	 (int)(di.size >> 20), (unsigned int)di.start, (unsigned int)di.regs);
  /* Enable EDAC */
  di.regs[8] = 1;

  /* Launch application tasks */
  i = rtems_task_create(rtems_build_name('r','e','p','t'), 200,
			RTEMS_MINIMUM_STACK_SIZE, 0, 0, &report_task);
  if (i != 0) rtems_fatal_error_occurred(i);
  i = rtems_task_create(rtems_build_name('f','i','n','j'), 150,
			RTEMS_MINIMUM_STACK_SIZE, 0, 0, &fault_task);
  if (i != 0) rtems_fatal_error_occurred(i);
  i = rtems_task_start(report_task, report_task_entry, 0);
  if (i != 0) rtems_fatal_error_occurred(i);  
  i = rtems_task_start(fault_task, fault_task_entry, 
		       (rtems_task_argument)(&di));
  if (i != 0) rtems_fatal_error_occurred(i);
  
  /* Remove init task */
  rtems_task_delete(RTEMS_SELF);
  rtems_fatal_error_occurred(0);
}
