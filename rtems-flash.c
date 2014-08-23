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

#ifdef RTEMS_DRVMGR_STARTUP
 #ifdef LEON3
  /* Add Timer and UART Driver for this example */
  #ifdef CONFIGURE_APPLICATION_NEEDS_CLOCK_DRIVER
   #define CONFIGURE_DRIVER_AMBAPP_GAISLER_GPTIMER
  #endif
  #ifdef CONFIGURE_APPLICATION_NEEDS_CONSOLE_DRIVER
   #define CONFIGURE_DRIVER_AMBAPP_GAISLER_APBUART
  #endif
 #endif
 #include <drvmgr/drvmgr_confdefs.h>
#endif

#include <stdio.h>
#include <stdlib.h>

#include "flashlib.h"

struct mctrl_regs {
	volatile unsigned int mcfg1;
	volatile unsigned int mcfg2;
	volatile unsigned int mcfg3;
};

struct flashlib_device fldev =
{
	.start = 0,
	.sector_count = 32,
	.sector_size = 64*1024,
};

struct flashlib_dev_info flinfo;

unsigned char pattern[1000];

rtems_task Init(
  rtems_task_argument ignored
)
{
	struct mctrl_regs *mctrl;
	void *handle;
	int i, status;

  printf("Testing FLASH routines for AM29F016 chip\n");

	/* Enable write cycles to FLASH, set PWEN bit
	 * Memory controller address is assumed.
	 */
	mctrl = (struct mctrl_regs *)0x80000000;
	mctrl->mcfg1 |= 1<<11;

	handle = flashlib_init(&fldev);
	if ( handle == NULL ) {
		printf("Failed to initialize FLASH Library\n");
	}

	flashlib_reset(handle);

	memset(&flinfo, 0, sizeof(flinfo));

	status = flashlib_info(&fldev, &flinfo);
	if ( status ) {
		printf("flashlib_info failed: %d\n", status);
		exit(-1);
	}

	printf("Manufacter:         0x%02x\n", flinfo.manufacter);
	printf("Device:             0x%02x\n", flinfo.device);
	for (i=0; i<32/4; i++) {
		printf("Sector Group %02d:    %s\n", i,
				flinfo.secgrp_protected[i] ? "Protected" : "Unlocked");
	}

#if 0
	/* Do chip Erase, by specifying a region which includes all the chip */
	status = flashlib_erase(handle, 0, 0x1ffff3);
	if ( status ) {
		printf("flashlib_erase (CHIP) failed: %d\n", status);
		exit(-1);
	}

	/* Do sector erase */
	status = flashlib_erase(handle, 0x0e0000, 0x155678);
	if ( status ) {
		printf("flashlib_erase (SECTOR) failed: %d\n", status);
		exit(-1);
	}
#endif

	/* Erase first sector */
	status = flashlib_erase(handle, 0x000000, 0x000000);
	if ( status ) {
		printf("flashlib_erase (SECTOR) failed: %d\n", status);
		exit(-1);
	}

	/* Program a test pattern */
	for ( i=0; i<1000; i++) {
		pattern[i] = (44+i) & 0xff;
	}

	status = flashlib_program(handle, 0x1000, 998, (char *)&pattern[1]);
	if ( status ) {
		printf("flashlib_program failed: %d\n", status);
		exit(-1);
	}

  exit( 0 );
}
