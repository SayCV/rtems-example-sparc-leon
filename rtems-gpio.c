/*
 * A RTEMS sample application using the GPIO Library and GRGPIO driver
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
#define CONFIGURE_DRIVER_AMBAPP_GAISLER_GRGPIO  /* GRGPIO driver */
#define CONFIGURE_DRIVER_AMBAPP_GAISLER_PCIF    /* PCI is for RASTA-IO GRETH */
#define CONFIGURE_DRIVER_AMBAPP_GAISLER_GRPCI   /* PCI is for RASTA-IO GRETH */
#define CONFIGURE_DRIVER_PCI_GR_RASTA_IO        /* GR-RASTA-IO PCI Target Driver */

#ifdef LEON2
  /* PCI support for AT697 */
  #define CONFIGURE_DRIVER_LEON2_AT697PCI
  /* AMBA PnP Support for GRLIB-LEON2 */
  #define CONFIGURE_DRIVER_LEON2_AMBAPP
#endif

#include <drvmgr/drvmgr_confdefs.h>

#include <stdio.h>
#include <stdlib.h>

#undef ENABLE_NETWORK
#undef ENABLE_NETWORK_SMC_LEON3

#include "config.c"

#include <gpiolib.h>

#define GPIO_PORT_NR (4)

void *port;

void gpio_isr(int irq, void *arg)
{
	struct gpiolib_config cfg;

	/* Mask away GPIO IRQ */
	cfg.mask = 0;
	cfg.irq_level = GPIOLIB_IRQ_LEVEL;
	cfg.irq_polarity = GPIOLIB_IRQ_POL_LOW;

	if ( gpiolib_set_config(port, &cfg) ){
		printf("Failed to configure gpio port %d\n", GPIO_PORT_NR);
		exit(0);
	}

	printk("GPIO_ISR: %d, GOT %d\n", irq, (int)arg);
}

rtems_task Init(
  rtems_task_argument ignored
)
{
	volatile unsigned char *p;
	unsigned int freq;
	struct gpiolib_config cfg;
	unsigned int val;

	/* Initialize Driver manager and Networking, in config.c */
	system_init();

	/* Print device topology */	
	rtems_drvmgr_print_topo();

	/* The drivers that use the GPIO Libarary have already initialized
	 * the libarary, however if no cores the drivers will not initialize
	 * it.
	 */
	if ( gpiolib_initialize() ) {
		printf("Failed to initialize GPIO libarary\n");
		exit(0);
	}

	/* Show all GPIO Ports available */
	gpiolib_show(-1, NULL);

	port = gpiolib_open(GPIO_PORT_NR);
	if ( port == NULL ){
		printf("Failed to open gpio port %d\n", GPIO_PORT_NR);
		exit(0);
	}

	/* Mask away GPIO IRQ */
	cfg.mask = 0;
	cfg.irq_level = GPIOLIB_IRQ_LEVEL;
	cfg.irq_polarity = GPIOLIB_IRQ_POL_LOW;

	if ( gpiolib_set_config(port, &cfg) ){
		printf("Failed to configure gpio port %d\n", GPIO_PORT_NR);
		exit(0);
	}

	if ( gpiolib_irq_disable(port) ){
		printf("Failed to disable IRQ on gpio port %d\n", GPIO_PORT_NR);
		exit(0);
	}

	if ( gpiolib_irq_clear(port) ){
		printf("Failed to clear IRQ on gpio port %d\n", GPIO_PORT_NR);
		exit(0);
	}

	if ( gpiolib_irq_register(port, gpio_isr, (void *)GPIO_PORT_NR) ){
		printf("Failed to register IRQ on gpio port %d\n", GPIO_PORT_NR);
		exit(0);
	}

	if ( gpiolib_irq_enable(port) ){
		printf("Failed to enable IRQ on gpio port %d\n", GPIO_PORT_NR);
		exit(0);
	}

	if ( gpiolib_get(port, &val) ){
		printf("Failed to get value of gpio port %d\n", GPIO_PORT_NR);
		exit(0);
	}
	printf("Current Value on GPIO: %d\n", val);

	/* Enable IRQ by unmasking IRQ */
	cfg.mask = 1;
	if ( gpiolib_set_config(port, &cfg) ){
		printf("Failed to configure gpio port %d\n", GPIO_PORT_NR);
		exit(0);
	}

	/* Print the same */
	gpiolib_show(GPIO_PORT_NR, NULL);
	gpiolib_show(0, port);

	gpiolib_close(port);

	exit( 0 );
}
