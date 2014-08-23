/* GR-RASTA-IO interface test example.
 * 
 * Gaisler Research 2007,
 * Daniel Hellström
 *
 */

#undef CANRX_ONLY
#define STATUS_PRINT

/*
#undef UART_TEST
#undef SPW_TEST
#undef CAN_TEST
#define BRM_TEST
*/
#define CONFIGURE_INIT
#include <bsp.h> /* for device driver prototypes */

rtems_task Init( rtems_task_argument argument);	/* forward declaration needed */

/* configuration information */
#define CONFIGURE_APPLICATION_NEEDS_CONSOLE_DRIVER
#define CONFIGURE_APPLICATION_NEEDS_CLOCK_DRIVER
#define CONFIGURE_APPLICATION_NEEDS_NULL_DRIVER 1
#define CONFIGURE_MAXIMUM_TASKS             16
#define CONFIGURE_RTEMS_INIT_TASKS_TABLE
#define CONFIGURE_EXTRA_TASK_STACKS         (20 * RTEMS_MINIMUM_STACK_SIZE)
#define CONFIGURE_LIBIO_MAXIMUM_FILE_DESCRIPTORS 32
#define CONFIGURE_INIT_TASK_PRIORITY	100
#define CONFIGURE_MAXIMUM_DRIVERS 16

/* Configure RTEMS Kernel */
#include <rtems/confdefs.h>

#include <drvmgr/drvmgr.h>

/* Configure Driver manager */
#if defined(RTEMS_DRVMGR_STARTUP) && defined(LEON3) /* if --drvmgr was given to configure */
 /* Add Timer and UART Driver for this example */
 #ifdef CONFIGURE_APPLICATION_NEEDS_CLOCK_DRIVER
  #define CONFIGURE_DRIVER_AMBAPP_GAISLER_GPTIMER
 #endif
 #ifdef CONFIGURE_APPLICATION_NEEDS_CONSOLE_DRIVER
  #define CONFIGURE_DRIVER_AMBAPP_GAISLER_APBUART
 #endif
#endif

#define CONFIGURE_DRIVER_AMBAPP_GAISLER_GRETH

/* Include SpaceWire driver */
#ifdef SPW_TEST
#define CONFIGURE_DRIVER_AMBAPP_GAISLER_GRSPW
#endif

/* Include CAN driver */
#ifdef CAN_TEST
#define CONFIGURE_DRIVER_AMBAPP_GAISLER_GRCAN
#endif

/* Include BRM driver */
#ifdef BRM_TEST
#define CONFIGURE_DRIVER_AMBAPP_GAISLER_B1553BRM
#endif

/* Include RAW UART driver for GR-RASTA-IO APBUART cores */
#ifdef UART_TEST
#error UART Test not supported at this point
#define CONFIGURE_DRIVER_AMBAPP_GAISLER_APBUART
#endif

#define CONFIGURE_DRIVER_AMBAPP_GAISLER_PCIF    /* PCI is for RASTA-IO GRETH */
#define CONFIGURE_DRIVER_AMBAPP_GAISLER_GRPCI   /* PCI is for RASTA-IO GRETH */
#define CONFIGURE_DRIVER_PCI_GR_RASTA_IO        /* GR-RASTA-IO has a GRETH network MAC */

#ifdef LEON2
  /* PCI support for AT697 */
  #define CONFIGURE_DRIVER_LEON2_AT697PCI
  /* AMBA PnP Support for GRLIB-LEON2 */
  #define CONFIGURE_DRIVER_LEON2_AMBAPP
#endif

#include <drvmgr/drvmgr_confdefs.h>

/* Application */
#include <rtems.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sched.h>
#include <ctype.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <rtems/rtems_bsdnet.h>

#include <gr_rasta_io.h>

/* This is used in config.c to set up networking */
#define ENABLE_NETWORK
#define ENABLE_NETWORK_SMC_LEON2
/* Define this is a SMC91C111 CHIP is available on the I/O bus */
/*#define ENABLE_NETWORK_SMC_LEON3*/

/* Include driver configurations and system initialization */
#include "config.c"

rtems_task status_task1(rtems_task_argument argument);

extern int can_init(void);
extern void can_start(void);
extern void can_print_stats(void);

extern int b1553_init(void);
extern void b1553_start(void);
extern void b1553_print_stats(void);

extern int spw_init(void);
extern void spw_start(void);
extern void spw_print_stats(void);

extern int uart_init(void);
extern void uart_start(void);
extern void uart_print_stats(void);


#ifdef STATUS_PRINT
int status_init(void);
void status_start(void);

static rtems_id   tds[1];        /* array of task ids */
static rtems_name tnames[1];     /* array of task names */
#endif

/* ========================================================= 
   initialisation */

rtems_task Init(
  rtems_task_argument ignored
)
{
  rtems_status_code status;
  
  printf("******** Starting RASTA test ********\n");

  /* Initialize Driver manager and Networking, in config.c */
  system_init();

#ifdef UART_TEST
  if ( uart_init() ){
    printf("UART INITIALIZATION FAILED, aborting\n");
    exit(1);
  }
#endif  
  
#ifdef CAN_TEST
  if ( can_init() ){
    printf("CAN INITIALIZATION FAILED, aborting\n");
    exit(1);
  }
#endif

#ifdef BRM_TEST
  if ( b1553_init() ){
    printf("BRM INITIALIZATION FAILED, aborting\n");
    exit(2);
  }
#endif

#ifdef SPW_TEST
  if ( spw_init() ){
    printf("SPW INITIALIZATION FAILED, aborting\n");
    exit(3);
  }
#endif

#ifdef STATUS_PRINT
  if ( status_init() ){
    printf("STATUS INITIALIZATION FAILED, aborting\n");
    exit(4);
  }
#endif
  

#ifdef UART_TEST
  uart_start();
#endif

#ifdef CAN_TEST
  can_start();
#endif

#ifdef BRM_TEST
  b1553_start();
#endif

#ifdef SPW_TEST
  spw_start();
#endif

#ifdef STATUS_PRINT
  status_start();
#endif

  status = rtems_task_delete(RTEMS_SELF);
}

#ifdef STATUS_PRINT

int status_init(void)
{
  int i;
  rtems_status_code status;
  
  for ( i=0; i<1; i++){
    tnames[i] = rtems_build_name( 'T', 'D', 'C', '0'+i );
  }
  
  status = rtems_task_create(
    tnames[0], 1, RTEMS_MINIMUM_STACK_SIZE * 4,
    RTEMS_DEFAULT_MODES | RTEMS_PREEMPT,
    RTEMS_DEFAULT_ATTRIBUTES, &tds[0]
    );
  if ( status != RTEMS_SUCCESSFUL )
    return -1;
  
  return 0;
}

void status_start(void)
{
  rtems_status_code status;
  
  printf("Starting status task1\n");
  
  /* Starting receiver first */
	status = rtems_task_start(tds[0], status_task1, 1);
}

/* TX Task */
rtems_task status_task1(
        rtems_task_argument unused
) 
{
  while(1){
    /* print stats */

#ifdef UART_TEST
    uart_print_stats();
    sched_yield();
#endif

#ifdef CAN_TEST
    can_print_stats();
    sched_yield();
#endif

#ifdef BRM_TEST
    b1553_print_stats();
    sched_yield();
#endif

#ifdef SPW_TEST
    spw_print_stats();
    sched_yield();
#endif

    sleep(2);
  }

}
#endif
