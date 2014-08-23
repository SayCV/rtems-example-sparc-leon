/*  RTEMS GR1553B BM driver usage example
 *
 *  COPYRIGHT (c) 2010.
 *  Aeroflex Gaisler.
 *
 *
 *  OVERVIEW
 *  --------
 *  Logger of 1553 bus, does not filter out anything, reads from a DMA area
 *  and compresses it into a larger log area. The larger log area can be read
 *  from a TCP/IP service using the linux_client.c example. The Linux
 *  application output it into a textfile for later processing.
 *
 *  The example is configured from config_bm.h and affects all applications
 *  using the BM: rtems-gr1553bm, rtems-gr1553bcbm rtems-gr1553rtbm
 *
 */

#include <rtems.h>

/* configuration information */

#define CONFIGURE_INIT

#include <bsp.h> /* for device driver prototypes */

rtems_task Init( rtems_task_argument argument);	/* forward declaration needed */

/* configuration information */

#define CONFIGURE_APPLICATION_NEEDS_CONSOLE_DRIVER
#define CONFIGURE_APPLICATION_NEEDS_CLOCK_DRIVER
#define CONFIGURE_MAXIMUM_TASKS             8
#define CONFIGURE_RTEMS_INIT_TASKS_TABLE
#define CONFIGURE_EXTRA_TASK_STACKS         (64 * RTEMS_MINIMUM_STACK_SIZE)
#define CONFIGURE_LIBIO_MAXIMUM_FILE_DESCRIPTORS 32
#define CONFIGURE_MAXIMUM_DRIVERS 16

#include <rtems/confdefs.h>

/* Include BM Log application configuration. We need to know if
 * the ethernet server is to be started, and if the log is to be
 * compressed.
 */
#include "config_bm.h"

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
#ifdef ETH_SERVER
#define CONFIGURE_DRIVER_AMBAPP_GAISLER_GRETH
#endif
#define CONFIGURE_DRIVER_PCI_GR_RASTA_IO        /* GR-RASTA-IO PCI Target Driver */
#define CONFIGURE_DRIVER_AMBAPP_GAISLER_GRSPW

/* CONFIGURE GR-RASTA-IO Board */
#define CONFIGURE_DRIVER_AMBAPP_MCTRL           /* Driver for Memory controller needed when using SRAM on PCI board */
#define RASTA_IO_SRAM

/******** ADD GR1553BM DRIVER **********/
#define CONFIGURE_DRIVER_AMBAPP_GAISLER_GR1553BM

#ifdef LEON2
  /* PCI support for AT697 */
  #define CONFIGURE_DRIVER_LEON2_AT697PCI
  /* AMBA PnP Support for GRLIB-LEON2 */
  #define CONFIGURE_DRIVER_LEON2_AMBAPP
#endif

#include <drvmgr/drvmgr_confdefs.h>

#include <stdio.h>
#include <stdlib.h>

/* If Ethernet server enable networking */
#ifdef ETH_SERVER
#define ENABLE_NETWORK
#else
#undef ENABLE_NETWORK
#endif

#undef ENABLE_NETWORK_SMC_LEON3

#include "config.c"

/* CONFIG: Define AMBA_OVER_PCI if BC is accessed over PCI, for example on a
 *         GR-RASTA-IO board.
 */
/*#define AMBA_OVER_PCI*/

#ifdef AMBA_OVER_PCI
  /* Bus Monitor (BM) LOGGING BASE ADDRESS : In SRAM of GR-RASTA-XXXX */
  #define BM_LOG_BASE (0x40010000 | 1)
#else
  /* Bus Monitor (BM) LOGGING BASE ADDRESS : Dynamically allocated */
  #define BM_LOG_BASE NULL
#endif

int init_bm(void);
int bm_log(void);
int bm_loop(void);

/* Ethernet Server functions */
extern int server_init(char *host, int port);
extern int server_wait_client(void);
extern int server_loop(void);
extern void server_stop();
volatile int client_avail = 0;

rtems_id taskEthid;
rtems_name taskEthname;

rtems_task Init(
  rtems_task_argument ignored
)
{

	/* Initialize Driver manager and Networking, in config.c */
	system_init();

	/* Print device topology */	
	rtems_drvmgr_print_topo();

	if ( init_bm() ) {
		printf("Failed to initialize BM\n");
		exit(0);
	}

	if ( bm_loop() ) {
		printf("Failed to log BM\n");
#ifdef ETH_SERVER
		 server_stop();
#endif
		exit(0);
	}

	exit( 0 );
}

int bm_loop(void)
{

	printf("Starting Copy of Log\n");

	while ( 1 ) {

		/* Handle Log buffer */
		if ( bm_log() ) {
			printf("BM Log failed\n");
			return -2;
		}

		/* Sleep one tick */
		rtems_task_wake_after(1);
	}

	return 0;
}

#include "bm_logger.c"
