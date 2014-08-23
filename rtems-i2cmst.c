/* Simple I2CMST example application
 *
 * Copyright (c) Gaisler Research 2009
 *
 */

#include <rtems.h>

#define CONFIGURE_INIT
#include <bsp.h> /* for device driver prototypes */
rtems_task Init( rtems_task_argument argument);	/* forward declaration needed */

/* configuration information */
#define CONFIGURE_APPLICATION_NEEDS_CONSOLE_DRIVER
#define CONFIGURE_APPLICATION_NEEDS_CLOCK_DRIVER
#define CONFIGURE_APPLICATION_NEEDS_NULL_DRIVER 1
#define CONFIGURE_MAXIMUM_TASKS             8
#define CONFIGURE_RTEMS_INIT_TASKS_TABLE
#define CONFIGURE_EXTRA_TASK_STACKS         (3 * RTEMS_MINIMUM_STACK_SIZE)
#define CONFIGURE_LIBIO_MAXIMUM_FILE_DESCRIPTORS 16
#define CONFIGURE_INIT_TASK_PRIORITY	100
#define CONFIGURE_USE_IMFS_AS_BASE_FILESYSTEM

#include <rtems/confdefs.h>

/* Select drivers used by the driver manager */
#if defined(RTEMS_DRVMGR_STARTUP) && defined(LEON3) /* if --drvmgr was given to configure */
 /* Add Timer and UART Driver for this example */
 #ifdef CONFIGURE_APPLICATION_NEEDS_CLOCK_DRIVER
  #define CONFIGURE_DRIVER_AMBAPP_GAISLER_GPTIMER
 #endif
 #ifdef CONFIGURE_APPLICATION_NEEDS_CONSOLE_DRIVER
  #define CONFIGURE_DRIVER_AMBAPP_GAISLER_APBUART
 #endif
#endif
#define CONFIGURE_DRIVER_AMBAPP_GAISLER_I2CMST

#ifdef LEON2
  /* AMBA PnP Support for GRLIB-LEON2 */
  #define CONFIGURE_DRIVER_LEON2_AMBAPP
#endif

/* Configure Driver Manager */
#include <drvmgr/drvmgr_confdefs.h>

#include <rtems/libi2c.h>
#include <libchip/i2c-2b-eeprom.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <i2cmst.h>

/* **** Test configuration **** */
/* 
   Both DO_I2C_RAWDRIVER_TEST and DO_I2C_HIGHLEVEL_TEST may be defined.
   The i2cmst driver has a DEBUG define that enables print outs of what
   is happening in the driver
*/
/* Address of EEPROM present in system: */
#define EEPROM_ADDR 0x50 
/* Use 'raw' driver to read/write 1B EEPROM: */
#define DO_I2C_RAWDRIVER_EXAMPLE
/* Use high-level 2B EEPROM driver to read/write 2B EEPROM: */
/*#define DO_I2C_HIGHLEVEL_EXAMPLE*/

/* Define USE_MULTIPLE_CORES if multiple i2cmst cores are available and
 * two cores should be used in the demo.
 *
 * Both Cores are assumed to be attached to similar eeproms.
 */
/*#define USE_MULTIPLE_CORES*/

/* Select I2C Master core used in example (the minor), valid range is 1..N, 
 * where N is the number of cores in the system 
 *
 * I2CMST_MINOR is always used
 * I2CMST_MINOR2 is used when two cores are used
 */
#define I2CMST_MINOR 1
#ifdef USE_MULTIPLE_CORES
#define I2CMST_MINOR2 2
#endif

/* **** End of test configuration **** */

/* Include driver configurations and system initialization */
#include "config.c"

rtems_task task1(rtems_task_argument argument);

rtems_id   Task_id[3];         /* array of task ids */
rtems_name Task_name[3];       /* array of task names */

/* initialisation */

rtems_task Init(rtems_task_argument ignored)
{
  rtems_status_code status;

	system_init();

	rtems_drvmgr_print_topo();
	rtems_drvmgr_print_devs(DRV_MGR_PRINT_DEVS_ALL);

  printf("******** Starting Gaisler I2CMST test ********\n");
	
	/* I2C MASTER has been initialized by the driver manager */

#if defined(DO_I2C_HIGHLEVEL_EXAMPLE)
  printf("Registering EEPROM driver: ");
  status = rtems_libi2c_register_drv("eeprom", i2c_2b_eeprom_driver_descriptor,
				     I2CMST_MINOR-1, EEPROM_ADDR);
  if (status < 0) {
    printf("ERROR: Could not register EEPROM driver\n");
    exit(0);
  }
  status = rtems_libi2c_register_drv("eeprom", i2c_2b_eeprom_driver_descriptor,
				     I2CMST_MINOR2-1, EEPROM_ADDR);
  if (status < 0) {
    printf("ERROR: Could not register EEPROM driver\n");
    exit(0);
  }
  printf("driver registered successfully\n");
#endif

  Task_name[1] = rtems_build_name( 'T', 'S', 'K', 'A' );

  status = rtems_task_create(
			     Task_name[1], 1, RTEMS_MINIMUM_STACK_SIZE * 2,
			     RTEMS_DEFAULT_MODES, 
			     RTEMS_DEFAULT_ATTRIBUTES, &Task_id[1]
			     );

  status = rtems_task_start(Task_id[1], task1, 1);

  status = rtems_task_delete(RTEMS_SELF);
}


/* Helper functions */

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

/* Test task */

int test_minor1(void) 
{

  int fd;
  int i;
  unsigned char buf[100];
#if defined(DO_I2C_RAWDRIVER_EXAMPLE)
  int status;
  dev_t raw_dev_t;
  /* We will create a /dev entry for the raw driver below and name it: */
  char *raw_dev = "/dev/i2c" "I2CMST_MINOR" "-50";
#endif
#if defined(DO_I2C_HIGHLEVEL_EXAMPLE)
  /* The registered bus is named i2c1 in the I2CMST driver and we named
     the eeprom driver "eeprom" above. The eeprom /dev name becomes: */
  char *eeprom_dev = "/dev/i2c" "I2CMST_MINOR" ".eeprom"; 
#endif
  

  printf("Starting test task...\n");

  /* **** Test with 'raw' interface **** */
#if defined(DO_I2C_RAWDRIVER_EXAMPLE)
  /* Create node for 'raw' access to EEPROM device */
  raw_dev_t = rtems_filesystem_make_dev_t(rtems_libi2c_major, 
				      RTEMS_LIBI2C_MAKE_MINOR(I2CMST_MINOR-1,EEPROM_ADDR));
  status = mknod(raw_dev, S_IFCHR | 0666, raw_dev_t);
  if (status < 0) {
    printf("mknod of %s failed: %s\n", raw_dev, strerror(errno));
    exit(0);
  }

  printf("Reading from EEPROM using 'raw' driver..\n");
  fd = ver_open(raw_dev, O_RDWR);

  /* Set address */
  buf[0] = 0;
  ver_write(fd, buf, 1);

  printf("Reading 100 bytes from EEPROM\n");
  ver_read(fd, buf, 100);
  printf("Contents of first 100 bytes of EEPROM:\n");
  printbuf(buf, 100, 0);

  printf("Writing 16 bytes to EEPROM, starting at address 0x00: ");
  buf[0] = 0; /* First will become address */
  for (i = 1; i < 17; i++)
    buf[i] = i-1;
  ver_write(fd, buf, 17);
  printf("done..\n");
  
  printf("Reading the first 20 bytes of the EEPROM:\n");
  buf[0] = 0;
  ver_write(fd, buf, 1);
  ver_read(fd, buf, 20);
  printbuf(buf, 20, 0);
  for (i = 0; i < 16; i++)
    if (buf[i] != i)
      printf("ERROR! Mismatch on byte %d\n", i);

  close(fd);
#endif

  /* **** Test with 'high level' interface **** */
#if defined(DO_I2C_HIGHLEVEL_EXAMPLE)
  printf("Reading EEPROM using high level driver..\n");
  fd = ver_open(eeprom_dev, O_RDWR);
  
  /* Read 100 bytes from EEPROM */
  printf("Reading 100 bytes from EEPROM\n");
  ver_read(fd, buf, 100);
  printf("Contents of first 100 bytes of EEPROM:\n");
  printbuf(buf, 100, 0);

  /* Write values 0 - 99 to byte positions 0 - 99 */
  printf("Writing the first 100 bytes with the range 0 to 99: ");
  for (i = 0; i < 15; i++) {
    buf[i] = i;
  }
  /* Reset pointer offset, EEPROM driver sets the offset to 0 at open */
  close(fd);
  fd = ver_open(eeprom_dev, O_RDWR);
  
  ver_write(fd, buf, 16);
  printf("done!\n");

  printf("Reading the first 40 bytes..\n");
  close(fd);
  fd = ver_open(eeprom_dev, O_RDWR);
  ver_read(fd, buf, 40);
  printf("Current contents of the first 40 bytes:\n");
  printbuf(buf, 40, 0);
  printf("Reading 60 bytes more..\n"); /* File position serves as address */
  ver_read(fd, buf, 60);
  printf("Contents of bytes 40 to 60:\n");
  printbuf(buf, 60, 40);
#endif
  printf("Test completed, no more output will come from "
	 "this test application\n");

	return 0;
}

#ifdef USE_MULTIPLE_CORES
int test_minor2(void) 
{

  int fd;
  int i;
  unsigned char buf[100];
#if defined(DO_I2C_RAWDRIVER_EXAMPLE)
  int status;
  dev_t raw_dev_t;
  /* We will create a /dev entry for the raw driver below and name it: */
  char *raw_dev = "/dev/i2c" "I2CMST_MINOR2" "-50";
#endif
#if defined(DO_I2C_HIGHLEVEL_EXAMPLE)
  /* The registered bus is named i2c1 in the I2CMST driver and we named
     the eeprom driver "eeprom" above. The eeprom /dev name becomes: */
  char *eeprom_dev = "/dev/i2c" "I2CMST_MINOR2" ".eeprom"; 
#endif
  

  printf("Starting test task...\n");

  /* **** Test with 'raw' interface **** */
#if defined(DO_I2C_RAWDRIVER_EXAMPLE)
  /* Create node for 'raw' access to EEPROM device */
  raw_dev_t = rtems_filesystem_make_dev_t(rtems_libi2c_major, 
				      RTEMS_LIBI2C_MAKE_MINOR(I2CMST_MINOR2-1,EEPROM_ADDR));
  status = mknod(raw_dev, S_IFCHR | 0666, raw_dev_t);
  if (status < 0) {
    printf("mknod of %s failed: %s\n", raw_dev, strerror(errno));
    exit(0);
  }

  printf("Reading from EEPROM using 'raw' driver..\n");
  fd = ver_open(raw_dev, O_RDWR);

  /* Set address */
  buf[0] = 0;
  ver_write(fd, buf, 1);

  printf("Reading 100 bytes from EEPROM\n");
  ver_read(fd, buf, 100);
  printf("Contents of first 100 bytes of EEPROM:\n");
  printbuf(buf, 100, 0);

  printf("Writing 16 bytes to EEPROM, starting at address 0x00: ");
  buf[0] = 0; /* First will become address */
  for (i = 1; i < 17; i++)
    buf[i] = i-1;
  ver_write(fd, buf, 17);
  printf("done..\n");
  
  printf("Reading the first 20 bytes of the EEPROM:\n");
  buf[0] = 0;
  ver_write(fd, buf, 1);
  ver_read(fd, buf, 20);
  printbuf(buf, 20, 0);
  for (i = 0; i < 16; i++)
    if (buf[i] != i)
      printf("ERROR! Mismatch on byte %d\n", i);

  close(fd);
#endif

  /* **** Test with 'high level' interface **** */
#if defined(DO_I2C_HIGHLEVEL_EXAMPLE)
  printf("Reading EEPROM using high level driver..\n");
  fd = ver_open(eeprom_dev, O_RDWR);
  
  /* Read 100 bytes from EEPROM */
  printf("Reading 100 bytes from EEPROM\n");
  ver_read(fd, buf, 100);
  printf("Contents of first 100 bytes of EEPROM:\n");
  printbuf(buf, 100, 0);

  /* Write values 0 - 99 to byte positions 0 - 99 */
  printf("Writing the first 100 bytes with the range 0 to 99: ");
  for (i = 0; i < 15; i++) {
    buf[i] = i;
  }
  /* Reset pointer offset, EEPROM driver sets the offset to 0 at open */
  close(fd);
  fd = ver_open(eeprom_dev, O_RDWR);
  
  ver_write(fd, buf, 16);
  printf("done!\n");

  printf("Reading the first 40 bytes..\n");
  close(fd);
  fd = ver_open(eeprom_dev, O_RDWR);
  ver_read(fd, buf, 40);
  printf("Current contents of the first 40 bytes:\n");
  printbuf(buf, 40, 0);
  printf("Reading 60 bytes more..\n"); /* File position serves as address */
  ver_read(fd, buf, 60);
  printf("Contents of bytes 40 to 60:\n");
  printbuf(buf, 60, 40);
#endif
  printf("Test completed, no more output will come from "
	 "this test application\n");

	return 0;
}
#endif

rtems_task task1(rtems_task_argument unused)
{
	if ( test_minor1() ) {
		printf("TEST1 FAILED\n");
		exit(-1);
	}
#ifdef USE_MULTIPLE_CORES
	if ( test_minor2() ) {
		printf("TEST1 FAILED\n");
		exit(-1);
	}
#endif
	printf("TEST OK\n");
	exit(0);
}
