/*
 * A RTEMS GR-RASTA-ADCDAC sample application using the PCIF, GRPCI or AT697 PCI driver
 * to access the PCI target. The application initializes demonstrates GPIO, ADCDAC and
 * GRETH together with AT697 SMC networking
 */

/* Define ADCDAC_DEMO to Enable ADC/DAC demo */
/*#define ADCDAC_DEMO*/
#define ADCDAC_DEMO_DEV "/dev/rastaadcdac0/gradcdac0"
/*#define ADCDAC_DEMO_DEV "/dev/rastaadcdac1/gradcdac0"*/

/* Define GPIO_TEST to enable GPIO test */
/*#define GPIO_TEST*/

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
#define CONFIGURE_EXTRA_TASK_STACKS         (32 * RTEMS_MINIMUM_STACK_SIZE)
#define CONFIGURE_LIBIO_MAXIMUM_FILE_DESCRIPTORS 32
#define CONFIGURE_MAXIMUM_DRIVERS 16

#include <rtems/confdefs.h>

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
#define CONFIGURE_DRIVER_AMBAPP_GAISLER_PCIF     /* PCI is for RASTA-IO GRETH */
#define CONFIGURE_DRIVER_AMBAPP_GAISLER_GRPCI    /* PCI is for RASTA-IO GRETH */
#define CONFIGURE_DRIVER_PCI_GR_RASTA_ADCDAC     /* GR-RASTA-ADCDAC PCI Target Driver */
#define CONFIGURE_DRIVER_AMBAPP_GAISLER_GRCAN    /* GRCAN CAN 2.0 DMA Driver */
#define CONFIGURE_DRIVER_AMBAPP_GAISLER_GRETH    /* GRETh Ethernet 10/100/1000 driver */

#ifdef LEON2
  /* PCI support for AT697 */
#ifndef LEON2_GRLIB
  #define CONFIGURE_DRIVER_LEON2_AT697PCI
  #define ENABLE_NETWORK_SMC_LEON2 /* AT697 board has LanChip MAC */
#endif
  /* AMBA PnP Support for GRLIB-LEON2 */
  #define CONFIGURE_DRIVER_LEON2_AMBAPP
#endif

#ifdef ADCDAC_DEMO
#define CONFIGURE_DRIVER_AMBAPP_GAISLER_GRADCDAC /* GRADCDAC ADC/DAC Driver */
#endif

#ifdef GPIO_TEST
#define CONFIGURE_DRIVER_AMBAPP_GAISLER_GRGPIO   /* GRPIO GPIO Driver */
#endif

#include <drvmgr/drvmgr_confdefs.h>

#include <stdio.h>
#include <stdlib.h>

#define ENABLE_NETWORK
#undef ENABLE_NETWORK_SMC_LEON3

#include "config.c"

#include <pci.h>


#ifdef CONFIGURE_DRIVER_PCI_GR_RASTA_ADCDAC
#include <gr_rasta_adcdac.h>
#endif

#ifdef ADCDAC_DEMO
#include <gradcdac.h>

extern int adcdac_init(char *adc_devname, char *dac_devname);
extern int adcdac_start(void);
extern void adcdac_print_stats(void);
#endif

#ifdef GPIO_TEST
extern int gpioTestMatrix(void);
#endif

rtems_task Init(
  rtems_task_argument ignored
)
{
	int status;

	/* Initialize Driver manager and Networking, in config.c */
	system_init();

	/* Print device topology */	
	rtems_drvmgr_print_topo();

	/* Print AMBA Bus */
/*	ambapp_print(ambapp_root, 10);*/

	/* Print PCI bus resources */
	pci_print();

#ifdef CONFIGURE_DRIVER_PCI_GR_RASTA_ADCDAC
	/* Print Info for GR-RASTA-ADCDAC PCI Target */
	gr_rasta_adcdac_print(RASTA_ADCDAC_OPTIONS_AMBA | RASTA_ADCDAC_OPTIONS_IRQ);
#endif

#ifdef CONFIGURE_DRIVER_AMBAPP_GAISLER_GRGPIO
	/* Print GPIO Info */
	gpiolib_show(-1, NULL);
#endif

#ifdef GPIO_TEST
	/* Do GPIO test */
	if ( (status = gpioTestMatrix()) != 0 ) {
		printf("GPIO test Failed: %d\n", status);
		exit(-1);
	}
#endif

#ifdef ADCDAC_DEMO
	gradcdac_print(NULL);
	if ( adcdac_init(ADCDAC_DEMO_DEV,NULL) ){
		printf("### Failed to init adcdac test\n");
		exit(-1);
	}
	if ( adcdac_start() ) {
		printf("### Failed to start ADC/DAC test\n");
		exit(-1);
	}
	while(1) {
		sleep(2);
		adcdac_print_stats();
	}
#endif

#ifdef ENABLE_NETWORK
	while(1) {
		/* Let user ping to test network... */
		sleep(1);
	}
#endif

	exit( 0 );
}
