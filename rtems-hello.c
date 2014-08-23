/*
 * A simple rtems program to show how to use configuration templates
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

/* If --drvmgr was enabled during the configuration of the RTEMS kernel */
#ifdef RTEMS_DRVMGR_STARTUP
 #ifdef LEON3
  /* Add Timer and UART Driver for this example */
  #ifdef CONFIGURE_APPLICATION_NEEDS_CLOCK_DRIVER
   #define CONFIGURE_DRIVER_AMBAPP_GAISLER_GPTIMER
  #endif
  #ifdef CONFIGURE_APPLICATION_NEEDS_CONSOLE_DRIVER
   #define CONFIGURE_DRIVER_AMBAPP_GAISLER_APBUART
  #endif

  /* OPTIONAL FOR GRLIB SYSTEMS WITH APBUART AS SYSTEM CONSOLE.
   *
   * Assign a specific UART the system console (syscon=1) and debug output
   * (dbgcon=1). Note that the default is to have APBUART0 as system/debug
   * console, one must override it by setting syscon=0 and/or dbgcon=0.
   * Note also that the debug console does not have to be on the same UART
   * as the system console.
   *
   * Determine if console driver should do be interrupt driven (mode=1)
   * or polling (mode=0).
   */
  #if 0
   #include <drvmgr/ambapp_bus.h>
   /* APBUART0 */
   struct rtems_drvmgr_key grlib_drv_res_apbuart0[] =
   {
   	{"mode", KEY_TYPE_INT, {(unsigned int)1}},
   	{"syscon", KEY_TYPE_INT, {(unsigned int)0}},
   	{"dbgcon", KEY_TYPE_INT, {(unsigned int)1}},
   	KEY_EMPTY
   };
   /* APBUART1 */
   struct rtems_drvmgr_key grlib_drv_res_apbuart1[] =
   {
  	{"mode", KEY_TYPE_INT, {(unsigned int)1}},
  	{"dbgcon", KEY_TYPE_INT, {(unsigned int)0}},
  	{"syscon", KEY_TYPE_INT, {(unsigned int)1}},
  	KEY_EMPTY
   };
   /* LEON3 System with driver configuration for 2 APBUARTs, the
    * the rest of the AMBA device drivers use their defaults.
    */
   struct rtems_drvmgr_drv_res grlib_drv_resources[] =
   {
   	{DRIVER_AMBAPP_GAISLER_APBUART_ID, 0, &grlib_drv_res_apbuart0[0]},
   	{DRIVER_AMBAPP_GAISLER_APBUART_ID, 1, &grlib_drv_res_apbuart1[0]},
   	RES_EMPTY
   };
  #endif
 #endif

 #include <drvmgr/drvmgr_confdefs.h>
#endif

#include <stdio.h>
#include <stdlib.h>

rtems_task Init(
  rtems_task_argument ignored
)
{
  printf( "Hello World\n" );
  exit( 0 );
}
