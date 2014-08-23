/*  Init
 *
 *  This routine is the initialization task for this test program.
 *
 *  Don't forget to change the IP addresses
 */

#define USE_HTTPD
#define USE_FTPD
/*#define TEST_INIT*/

#define CONFIGURE_APPLICATION_NEEDS_CONSOLE_DRIVER
#define CONFIGURE_APPLICATION_NEEDS_CLOCK_DRIVER
#define CONFIGURE_RTEMS_INIT_TASKS_TABLE
#define CONFIGURE_LIBIO_MAXIMUM_FILE_DESCRIPTORS	20
#define CONFIGURE_USE_IMFS_AS_BASE_FILESYSTEM

#define CONFIGURE_EXECUTIVE_RAM_SIZE	(512*1024)
#define CONFIGURE_MAXIMUM_SEMAPHORES	20
#define CONFIGURE_MAXIMUM_TASKS		20

#define CONFIGURE_MICROSECONDS_PER_TICK	10000

#define CONFIGURE_INIT_TASK_STACK_SIZE	(10*1024)
#define CONFIGURE_INIT_TASK_PRIORITY	120
#define CONFIGURE_INIT_TASK_INITIAL_MODES (RTEMS_PREEMPT | \
                                           RTEMS_NO_TIMESLICE | \
                                           RTEMS_NO_ASR | \
                                           RTEMS_INTERRUPT_LEVEL(0))

#define CONFIGURE_INIT

#include <rtems.h>
#include <rtems/untar.h>
#include <bsp.h>

/* functions */

rtems_task Init(
  rtems_task_argument argument
);

/* configuration information */

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
#include <errno.h>
#include <time.h>

#include <stdio.h>
#include <rtems/rtems_bsdnet.h>
#include <rtems/ftpd.h>
     
#include <rtems/error.h>
#include <rpc/rpc.h>
#include <netinet/in.h>
#include <time.h>

#include <arpa/inet.h>
#include <sys/socket.h>

#include <rtems_webserver.h>

/* This is used in config.c to set up networking */
#define ENABLE_NETWORK
#define ENABLE_NETWORK_SMC_LEON2
/* Define this is a SMC91C111 CHIP is available on the I/O bus */
/*#define ENABLE_NETWORK_SMC_LEON3*/

/* Include driver configrations and system initialization */
#include "config.c"

#define ARGUMENT 0

/*
 *  The tarfile is built in $(ARCH) so includes whether we were
 *  built optimized or debug.
 */

#if defined(USE_FTPD)
/*
#if defined(RTEMS_DEBUG)
extern int _binary_o_debug_tarfile_start;
extern int _binary_o_debug_tarfile_size;
#define TARFILE_START _binary_o_debug_tarfile_start
#define TARFILE_SIZE _binary_o_debug_tarfile_size
#else
extern int _binary_o_optimize_tarfile_start;
extern int _binary_o_optimize_tarfile_size;
#define TARFILE_START _binary_o_optimize_tarfile_start
#define TARFILE_SIZE _binary_o_optimize_tarfile_size
#endif
*/
extern int _binary_tarfile_start;
extern int _binary_tarfile_size;
#define TARFILE_START _binary_tarfile_start
#define TARFILE_SIZE _binary_tarfile_size
#endif

#if defined(USE_FTPD)
struct rtems_ftpd_configuration rtems_ftpd_configuration = {
   10,                     /* FTPD task priority            */
   1024,                   /* Maximum buffersize for hooks  */
   21,                     /* Well-known port     */
   NULL                    /* List of hooks       */
};
#endif
rtems_task Init(
  rtems_task_argument argument
)
{
  rtems_status_code status;

  printf("\n\n*** HTTP TEST ***\n\r" );

  /* init_paging(); */

  /* Initialize Driver manager and Networking, in config.c */
  system_init();

#if defined(USE_FTPD)
  status = rtems_initialize_ftpd();
  if ( status != RTEMS_SUCCESSFUL ) {
    printf("FAILED TO INITIALIZE FTP SERVER\n");
  }

  status = Untar_FromMemory(
    (unsigned char *)(&TARFILE_START), (unsigned long)&TARFILE_SIZE);
#endif
   
#if defined(USE_HTTPD)
  status = rtems_initialize_webserver();
  if ( status != RTEMS_SUCCESSFUL ) {
    printf("FAILED TO INITIALIZE HTTP SERVER\n");
  }
#endif

  /*
  rtems_monitor_init (0);
  rtems_capture_cli_init (0);
  */
  status = rtems_task_delete( RTEMS_SELF );
}




