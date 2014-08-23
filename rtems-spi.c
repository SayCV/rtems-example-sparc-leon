/* SPICTRL interface example accessing a Intel SPI Flash.
 *
 * It will perform reads at addresses 0 (15 bytes), 15 (7 bytes), 
 * 0x10000 (4 bytes), 0x1fffa (10 bytes). It will do write accesses
 * to addresses 0x1fffc (5 bytes), 3 (10 bytes).
 *
 * Note that the flash is not erased, so writes may have not effect
 * only bits in the flash set to 1 can be programmed.
 *
 * Copyright (C),
 * Aeroflex Gaisler 2009,
 *
 */

/* Define this to use GPIO for chip select */
/*#define USE_GPIO_SLVSEL*/

#include <rtems.h>
#define CONFIGURE_INIT
#include <bsp.h> /* for device driver prototypes */

rtems_task Init( rtems_task_argument argument);	/* forward declaration needed */

/* configure RTEMS kernel */
#define CONFIGURE_APPLICATION_NEEDS_CONSOLE_DRIVER
#define CONFIGURE_APPLICATION_NEEDS_CLOCK_DRIVER
#define CONFIGURE_MAXIMUM_TASKS             16
#define CONFIGURE_RTEMS_INIT_TASKS_TABLE
#define CONFIGURE_EXTRA_TASK_STACKS         (24 * RTEMS_MINIMUM_STACK_SIZE)
#define CONFIGURE_LIBIO_MAXIMUM_FILE_DESCRIPTORS 32
#define CONFIGURE_INIT_TASK_PRIORITY	100
#define CONFIGURE_MAXIMUM_DRIVERS 16
#define CONFIGURE_USE_IMFS_AS_BASE_FILESYSTEM

/* Configure RTEMS kernel */
#include <rtems/confdefs.h>

/* Configure Driver Manager Kernel */
#include <drvmgr/drvmgr.h>
#if defined(RTEMS_DRVMGR_STARTUP) && defined(LEON3) /* if --drvmgr was given to configure */
 /* Add Timer and UART Driver for this example */
 #ifdef CONFIGURE_APPLICATION_NEEDS_CLOCK_DRIVER
  #define CONFIGURE_DRIVER_AMBAPP_GAISLER_GPTIMER
 #endif
 #ifdef CONFIGURE_APPLICATION_NEEDS_CONSOLE_DRIVER
  #define CONFIGURE_DRIVER_AMBAPP_GAISLER_APBUART
 #endif
#endif
#define CONFIGURE_DRIVER_AMBAPP_GAISLER_SPICTRL
#ifdef USE_GPIO_SLVSEL
#define CONFIGURE_DRIVER_AMBAPP_GAISLER_GRGPIO
#endif
#include <drvmgr/drvmgr_confdefs.h>

#include <spictrl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>

/* No networking */
#undef ENABLE_NETWORK_SMC_LEON3
#undef ENABLE_NETWORK_SMC_LEON2
#undef ENABLE_NETWORK

#ifdef USE_GPIO_SLVSEL
#include <gpiolib.h>
/* Provide custom slave select function */
void *spi_gpio_port = NULL;
extern int spi_gpio_slvsel(void *regs, int addr, int select);
extern int spi_gpio_slvsel_setup(int port);
#define SPICTRL_SLV_SEL_FUNC spi_gpio_slvsel
#define GPIO_PORT_NR 5
#endif

/* Include driver configurations and system initialization */
#include "config.c"


#include <rtems.h>
#include <rtems/libi2c.h>
#include <libchip/spi-memdrv.h>
#include <rtems/libio.h>

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

/* Tell SPI_MEMDRV about how the Flash is set up */
spi_memdrv_t intel_spi_flash = {
  {/* public fields */
  ops:         &spi_memdrv_rw_ops, /* operations of general memdrv */
  size:        sizeof (intel_spi_flash),
  },
  { /* our private fields */
  baudrate:             2000000,
  erase_before_program: true,
  empty_state:          0xff,
  page_size:            256, /* programming page size in byte */
  sector_size:          64*1024, /* erase sector size in byte */
  mem_size:             8*1024*1024 /* total capacity in byte    */
  }
};

rtems_libi2c_drv_t *intel_spi_flash_desc = 
&intel_spi_flash.libi2c_drv_entry;

/* ver_read: Verbose read, checks return value of read() call */
void ver_read(int fd, void *buf, size_t count)
{
  int status;

  status = read(fd, buf, count);
  if (status < 0) {
    printf("read() call returned with error: %s\n",
	   strerror(errno));

  } else if (status < count) {
    printf("\nread() did not return with all requested bytes\n"
	   "requested bytes: %d, returned bytes: %d\n\n",
	   count, status);
  }
}

void ver_write(int fd, const void *buf, size_t count)
{
  int status;
  
  status = write(fd, buf, count);
  if (status < 0) {
    printf("write() call returned with error: %s\n",
	   strerror(errno));
  } else if (status < count) {
    printf("write() did not output all bytes\n"
	   "requested bytes: %d, returned bytes: %d\n\n",
	   count, status);
  }
}

/* ver_open: Checks return value of open and exits on failure */
int ver_open(const char *pathname, int flags)
{
  int fd;
  
  fd = open(pathname, O_RDWR);
  if (fd < 0) {
    printf("Could not open %s: %s\n", pathname, strerror(errno));
    exit(0);
  }
  return fd;
}

/* printbuf: Produces formatted printout of 'len' bytes from buffer 'buf' */
void printbuf(unsigned char *buf, int len, int saddr)
{
  int i;

  for (i = 0; i < len; i++) {
    if (i % 4 == 0)
      printf("0x%02X:", saddr + i);
    printf("\t%x%s", buf[i], (i+1) % 4 == 0 ? "\n" : "");
  }
}


/* ========================================================= 
   initialisation */

rtems_task Init(
  rtems_task_argument ignored
)
{
	rtems_status_code status;
	char *spi_flash_dev = "/dev/spi1.flash"; 
	int fd;
	unsigned char rxbuf[256], txbuf[256];
	off_t ofs;

	printf("******** Initializing SPICTRL test ********\n");

  /* Initialize Driver manager, in config.c */
	system_init();

#ifdef USE_GPIO_SLVSEL
	spi_gpio_slvsel_setup(GPIO_PORT_NR);
#endif
/*
	rtems_drvmgr_print_devs(0xfffff);
	rtems_drvmgr_print_topo();
*/
	
	/* The driver has initialized the i2c library for us */

	printf("Registering SPI FLASH driver: ");
	status = rtems_libi2c_register_drv("flash", intel_spi_flash_desc,
	                                   0, 1);
	if (status < 0) {
		printf("ERROR: Could not register SPI FLASH driver\n");
		exit(0);
	}
	printf("driver registered successfully\n");

	fd = open(spi_flash_dev, O_RDWR);
	if ( fd < 0 ) {
		printf("Failed to open FLASH\n");
		exit(0);
	}

	/*** Read from current posistion (address 0x0) ***/
	printf("\n\nREAD 0x0\n");
	memset(rxbuf, 0, sizeof(rxbuf));
	ver_read(fd, &rxbuf[0], 15);
	printf("Read, bytes: %d [0x%x, 0x%x, 0x%x, 0x%x]\n", 15, rxbuf[0], rxbuf[1], rxbuf[2], rxbuf[3]);
	printbuf(rxbuf, 15, 0);

	/*** Read from current posistion (address 0xf due to previous read length) ***/
	printf("\n\nREAD 0xf\n");
	memset(rxbuf, 0, sizeof(rxbuf));
	ver_read(fd, &rxbuf[0], 7);
	printf("Read, bytes: %d [0x%x, 0x%x]\n", 7, rxbuf[0], rxbuf[1]);
	printbuf(rxbuf, 7, 0xf);

	/*** Read 4 bytes at 0x10000 by setting address using the lseek function ***/
	ofs = 0x10000;
	if ( lseek(fd, ofs, SEEK_SET) == (off_t)-1 ) {
		printf("Seek1 Failed %d (%s)\n", errno, strerror(errno));
	}
	printf("\n\nREAD 0x10000\n");
	memset(rxbuf, 0, sizeof(rxbuf));
	ver_read(fd, &rxbuf[0], 4);
	printf("Read, bytes: %d [0x%x, 0x%x]\n", 4, rxbuf[0], rxbuf[1]);
	printbuf(rxbuf, 4, 0x10000);

	/*** Read across page boundaries, read 10 bytes at address 0x1fffa ***/
	ofs = 0x1fffa;
	if ( lseek(fd, ofs, SEEK_SET) == (off_t)-1 ) {
		printf("Seek2 Failed %d (%s)\n", errno, strerror(errno));
	}
	printf("\n\nREAD 0x1fffa\n");
	memset(rxbuf, 0, sizeof(rxbuf));
	ver_read(fd, &rxbuf[0], 10);
	printf("Read, bytes: %d [0x%x, 0x%x]\n", 10, rxbuf[0], rxbuf[1]);
	printbuf(rxbuf, 10, 0x1fffa);

	/*** WRITE content of previous read bytes added with a constant ***/
	printf("\n\nWRITE 0x1fffc\n");
	ofs = 0x1fffc;
	if ( lseek(fd, ofs, SEEK_SET) == (off_t)-1 ) {
		printf("Seek3 Failed %d (%s)\n", errno, strerror(errno));
	}
	txbuf[0] = rxbuf[0xc - 0xa] + 1;
	txbuf[1] = rxbuf[0xd - 0xa] + 3;
	txbuf[2] = rxbuf[0xe - 0xa] + 5;
	txbuf[3] = rxbuf[0xf - 0xa] + 25;
	txbuf[4] = rxbuf[0x10 - 0xa] + 0x10;
	txbuf[5] = rxbuf[0x11 - 0xa] + 0x22;
	ver_write(fd, &txbuf[0], 5);

	/*** WRITE "rtems-spi" to address 0x3 ***/
	printf("\n\nWRITE 0x03\n");
	ofs = 0x3;
	if ( lseek(fd, ofs, SEEK_SET) == (off_t)-1 ) {
		printf("Seek3 Failed %d (%s)\n", errno, strerror(errno));
	}
	txbuf[0] = 'r';
	txbuf[1] = 't';
	txbuf[2] = 'e';
	txbuf[3] = 'm';
	txbuf[4] = 's';
	txbuf[5] = '-';
	txbuf[6] = 's';
	txbuf[7] = 'p';
	txbuf[8] = 'i';
	txbuf[9] = '\0';
	ver_write(fd, &txbuf[0], 10);

	exit(0);
}

#ifdef USE_GPIO_SLVSEL
int spi_gpio_slvsel_setup(int portnr)
{
	struct gpiolib_config cfg;

	/* The drivers that use the GPIO Libarary have already initialized
	 * the libarary, however if no cores the drivers will not initialize
	 * it.
	 */
	if ( gpiolib_initialize() ) {
		printf("Failed to initialize GPIO libarary\n");
		return -1;
	}

	/* Show all GPIO Ports available */
	gpiolib_show(-1, NULL);

	spi_gpio_port = gpiolib_open(portnr);
	if ( spi_gpio_port == NULL ){
		printf("Failed to open gpio port %d\n", portnr);
		return -1;
	}

	/* Mask IRQ, Low level (interrupt not enabled anyway) */
	cfg.mask = 0;
	cfg.irq_level = 1;
	cfg.irq_polarity = 0;
	gpiolib_set_config(spi_gpio_port, &cfg);

	/* Drive pin High */
	gpiolib_set(spi_gpio_port, 1, 1);

	return 0;
}

int spi_gpio_slvsel(void *regs, int addr, int select)
{
	/*printk("SPI ADDR: addr 0x%x [0x%x]\n", addr, regs);*/
	if ( (addr >= 0) && select ) {
		/* Drive low */
		gpiolib_set(spi_gpio_port, 1, 0);
	} else {
		/* Drive high */
		gpiolib_set(spi_gpio_port, 1, 1);
	}
	return 0;
}
#endif
