/*
 * A simple rtems program to show how to use the GRLIB GPTIMER watchdog
 * function. There is no output in the application, instead GRMON can be
 * used to watch the watchdog function doing the job:
 *
 * grmon> lo rtems-watchdog
 * grmon> watch 0x800003N4 w
 * grmon> run
 * ..
 *  BREAK
 * grmon> info reg
 * ...
 *  View watchdog registers
 * grmon> cont
 * 
 * where N=[1,2,3,4,5,6,7,8] is the last timer, the timer with the watchdog
 * functionality, 'info sys' in GRMON will tell the number of timers and
 * 'info reg' will show the current values and the addresses of the timer
 * registers.
 */

#include <rtems.h>
/* configuration information */

#define CONFIGURE_INIT

#include <bsp.h> /* for device driver prototypes */

rtems_task Init( rtems_task_argument argument);	/* forward declaration needed */

/* configuration information */

#define CONFIGURE_APPLICATION_NEEDS_CONSOLE_DRIVER
#define CONFIGURE_APPLICATION_NEEDS_CLOCK_DRIVER

#define CONFIGURE_MAXIMUM_TASKS             4

#define CONFIGURE_RTEMS_INIT_TASKS_TABLE

#define CONFIGURE_EXTRA_TASK_STACKS         (3 * RTEMS_MINIMUM_STACK_SIZE)


#include <rtems/confdefs.h>

#if defined(RTEMS_DRVMGR_STARTUP) /* if --drvmgr was given to configure */
 #error Example does not support this mode 
#endif

#include <stdio.h>
#include <stdlib.h>

#include <leon.h>

rtems_task Init(
  rtems_task_argument ignored
)
{

  printf( "Watchdog app starting\n" );

	/* First Time the watchdog function is called initialization is done */

	while ( 1 ) {

		bsp_watchdog_reload(0, 0x1fffff);  /* Calc time needed depending on work required */

		/* Do work here, or let other threads do work */
		sleep(2);

		bsp_watchdog_reload(0, 0xfffff);  /* Calc time needed depending on work required */

		/* Do less work here, or let other threads do work */
		sleep(1);

	}

  exit( 0 );
}
