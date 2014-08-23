/*
 * A RTEMS sample application using the PCIF, GRPCI or AT697 PCI driver
 *
 * The example add a custom PCI driver to the driver manager, the PCI
 * hardware ID must be changed in order for the driver to detect 
 * the board: PCIID_VENDOR_CUSTOM and PCIID_DEVICE_CUSTOM
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
#define CONFIGURE_DRIVER_AMBAPP_GAISLER_PCIF    /* GRLIB PCIF Host driver  */
#define CONFIGURE_DRIVER_AMBAPP_GAISLER_GRPCI   /* GRPCI Host driver */
#define CONFIGURE_DRIVER_AMBAPP_GAISLER_GRPCI2  /* GRPCI2 Host Driver */
#define CONFIGURE_DRIVER_PCI_GR_RASTA_IO        /* GR-RASTA-IO PCI Target Driver */
#define CONFIGURE_DRIVER_PCI_GR_701             /* GR-701 PCI Target Driver */
#define CONFIGURE_DRIVER_PCI_GR_RASTA_ADCDAC    /* GR-RASTA-ADCDAC PCI Target Driver */
/*#define CONFIGURE_DRIVER_PCI_GR_RASTA_TMTC*/      /* GR-RASTA-TMTC PCI Target Driver */


/******** ADD A CUSTOM PCI DRIVER **********/
/* Uncomment the line below to add the custom driver, don't forget to 
 * set PCIID_VENDOR_CUSTOM and PCIID_DEVICE_CUSTOM
 */
//#define CONFIGURE_DRIVER_CUSTOM1

#define DRIVER_CUSTOM1_REG {custom_pci_board_register_drv}
void custom_pci_board_register_drv(void);
/*******************************************/

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

rtems_task Init(
  rtems_task_argument ignored
)
{

	/* Initialize Driver manager and Networking, in config.c */
	system_init();

	/* Print device topology */	
	rtems_drvmgr_print_topo();

	printf("\n\n ####### PCI CONFIGURATION #######\n\n");

	/* Print PCI bus resources */
	pci_print();

#ifdef LEON3

	printf("\n\n ####### AMBA PnP CONFIGURATION #######\n\n");

	/* Print AMBA Bus */
	ambapp_print(&ambapp_plb, 10);
#endif

	exit( 0 );
}

/********** A CUSTOM PCI DRIVER ***********/
#include <pci.h>
#include <drvmgr/pci_bus.h>

/* CHANGE THIS !!! to you match the DEVIC/VENDOR of you PCI board */
#define PCIID_VENDOR_CUSTOM 1
#define PCIID_DEVICE_CUSTOM 1

rtems_status_code custom_pci_board_init1(struct rtems_drvmgr_dev_info *dev);
rtems_status_code custom_pci_board_init2(struct rtems_drvmgr_dev_info *dev);
#define DRIVER_PCI_VENDOR_DEVICE_ID	DRIVER_PCI_ID(PCIID_VENDOR_CUSTOM, PCIID_DEVICE_CUSTOM)

struct rtems_drvmgr_drv_ops custom_pci_board_ops =
{
    custom_pci_board_init1,
    custom_pci_board_init2
};

struct pci_dev_id custom_pci_board_ids[] =
{
    {PCIID_VENDOR_CUSTOM, PCIID_DEVICE_CUSTOM},
    {0, 0}        /* Mark end of table */
};

struct pci_drv_info custom_pci_board_info =
{
    {
        NULL,                /* Next driver */
        NULL,                /* Device list */
        DRIVER_PCI_VENDOR_DEVICE_ID,/* Driver ID */
        "CUSTOM_PCI_DRV",        /* Driver Name */
        DRVMGR_BUS_TYPE_PCI,        /* Bus Type */
        &custom_pci_board_ops,
        0,                /* No devices yet */
    },
    &custom_pci_board_ids[0]
};

void custom_pci_board_register_drv(void)
{
	printf("Registering CUSTOM PCI driver\n");
	rtems_drvmgr_drv_register(&custom_pci_board_info.general);
}

rtems_status_code custom_pci_board_init1(struct rtems_drvmgr_dev_info *dev)
{
	/* Nothing should be done here, unless we provide other Driver manager buses
	 * or other drivers in init2 depends upon functionality provided by this
	 * driver.
	 *
	 * One may want to reset the hardware here, as early as possible...
	 */

	return 0;
}

rtems_status_code custom_pci_board_init2(struct rtems_drvmgr_dev_info *dev)
{
	/* Initialize the PCI board hardware and driver, register interrupt
	 * routines, etc.
	 */


	struct pci_dev_info *devinfo;
	unsigned int bars[6];
	int bus, device, func;
	char irqno;

	devinfo = (struct pci_dev_info *)dev->businfo;

	bus = devinfo->bus;
	device = devinfo->dev;
	func = devinfo->func;

	memset(bars, 0, sizeof(bars));

	pci_read_config_dword(bus, device, func, PCI_BASE_ADDRESS_0, &bars[0]);
	pci_read_config_dword(bus, device, func, PCI_BASE_ADDRESS_1, &bars[1]);
	pci_read_config_dword(bus, device, func, PCI_BASE_ADDRESS_2, &bars[2]);
	pci_read_config_dword(bus, device, func, PCI_BASE_ADDRESS_3, &bars[3]);
	pci_read_config_dword(bus, device, func, PCI_BASE_ADDRESS_4, &bars[4]);
	pci_read_config_dword(bus, device, func, PCI_BASE_ADDRESS_5, &bars[5]);	
	pci_read_config_byte(bus, device, func, PCI_INTERRUPT_LINE, &irqno);

	printf("CUSTOM PCI: BAR0: 0x%08x\n", bars[0]);
	printf("CUSTOM PCI: BAR1: 0x%08x\n", bars[1]);
	printf("CUSTOM PCI: BAR2: 0x%08x\n", bars[2]);
	printf("CUSTOM PCI: BAR3: 0x%08x\n", bars[3]);
	printf("CUSTOM PCI: BAR4: 0x%08x\n", bars[4]);
	printf("CUSTOM PCI: BAR5: 0x%08x\n", bars[5]);
	printf("CUSTOM PCI: IRQ:  %d\n", irqno);

	return 0;
}

/*******************************************/
