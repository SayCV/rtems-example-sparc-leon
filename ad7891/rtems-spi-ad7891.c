/* Example of AD7891 ADC chip, it is accessed over SPI. Using standard or
 * periodic mode.
 *
 * Copyright (C),
 * Aeroflex Gaisler 2009,
 *
 */
 
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
#include <drvmgr/drvmgr_confdefs.h>

#include <spictrl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>

#undef ENABLE_NETWORK_SMC_LEON3
#undef ENABLE_NETWORK_SMC_LEON2
#undef ENABLE_NETWORK
/* Include driver configurations and system initialization */
#include "config.c"

#include "spi-ad78xx.h"

void ad7891_periodic_reads(int fd);
void ad7891_single_reads(int fd);

/* Tell SPI_AD7891 about how the Flash is set up */
spi_ad78xx_drv_t ad7891_spi_adc = {
  {/* public fields */
    ops:         &spi_ad78xx_ro_ops, /* operations of general memdrv */
    size:        sizeof (ad7891_spi_adc),
  },
  { /* our private fields */
    baudrate:             10000000,
    device_id:            7891,
    channels:             8,
    channel_bits:         3,
    abits:                12,

    /* Driver private */
    channel:              0,
    periodic_mode:        0,
    periodic_channel_cnt: 0,
    periodic_started:     0,
    periodic_exttrig:     0,
  }
};

rtems_libi2c_drv_t *ad7891_spi_adc_desc = &ad7891_spi_adc.libi2c_drv_entry;

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

/* ========================================================= 
   initialisation */

/* MCC ADC0 Channel names */
char *chan_name[8] = 
{
	"5V",
	"3V3",
	"1V5",
	"CURRENT",
	"TEMP",
	"ADC0_5",
	"ADC0_6",
	"ADC0_7"
};

rtems_task Init(
  rtems_task_argument ignored
)
{
	rtems_status_code status;
	char spi_ad7891_dev[30];
	int fd;
	off_t ofs;

	printf("******** Initializing AD7891 test ********\n");

  /* Initialize Driver manager, in config.c */
	system_init();

/*
	rtems_drvmgr_print_devs(0xfffff);
	rtems_drvmgr_print_topo();
*/

	sprintf(spi_ad7891_dev, "/dev/spi%d.ad", CHAN_NUM); 

	/* The driver has initialized the i2c library for us 
	 * 
	 * Register only one AD7891 chip, selected by CHAN_NUM=[1,2,3,4]
	 */

	printf("Registering SPI FLASH driver: ");
	status = rtems_libi2c_register_drv("ad", ad7891_spi_adc_desc,
	                                   CHAN_NUM-1, 1);
	if (status < 0) {
		printf("ERROR: Could not register SPI AD7891 driver\n");
		exit(0);
	}
	printf("driver registered successfully\n");

	fd = open(spi_ad7891_dev, O_RDWR);
	if ( fd < 0 ) {
		printf("Failed to open AD7891: %d\n", errno);
		exit(0);
	}

#ifdef SINGLE_READ
  ad7891_single_reads(fd);
#else
  ad7891_periodic_reads(fd);
#endif
  exit(0);
}

/* Read ADC values using standard (non-periodic) mode */
void ad7891_single_reads(int fd)
{
  unsigned short rxbuf[64];
	int channum, prevchannum;
	printf("\n\n\n\nPrinting values for SPI CORE %d (SINGLE)\n", CHAN_NUM);
	printf("CHAN: [I2C_0,I2C_1](VAL_HEX : VAL_DEC)\n");

	channum = 0; /*start at chan 1 */
	prevchannum = -1;
	while ( 1 ) {

		/* Select channel for next transfer */
    ioctl(fd, AD78XX_CONFIG_NEXT_CHAN, channum);

		memset(rxbuf, 0, sizeof(rxbuf));
		ver_read(fd, &rxbuf[0], 2);
		if ( prevchannum == 0 )
			printf("\n");
		printf("%d: [0x%04x](0x%x : %d)\n", 
			prevchannum,
			rxbuf[0],
		  (rxbuf[0] & 0xfff),
			(rxbuf[0] & 0xfff));

		prevchannum = channum;
		channum++;
		if ( channum >= 8 ) {
			channum = 0;
		}
	}

	return;
}

/* Read ADC values using periodic mode */
void ad7891_periodic_reads(int fd)
{
  unsigned short rxbuf[64];
	int channum, prevchannum;
  int i;
  struct spi_ad78xx_periodic_cfg percfg;

  printf("\n\n\n\nPrinting values for SPI CORE %d (PERIODIC)\n", CHAN_NUM);
  printf("CHAN: [I2C_0,I2C_1](VAL_HEX : VAL_DEC)\n");

  /* Set up all channels */
  percfg.enable_periodic = 1;
  percfg.periodic_exttrig = 0;
  percfg.channel_cnt = 8;
  percfg.channels[0] = 0;
  percfg.channels[1] = 1;
  percfg.channels[2] = 2;
  percfg.channels[3] = 3;
  percfg.channels[4] = 4;
  percfg.channels[5] = 5;
  percfg.channels[6] = 6;
  percfg.channels[7] = 7;

  if ( ioctl(fd, AD78XX_PERIOD_START, &percfg) ) {
    printf("Failed to configure periodic mode for AD78xx: %d\n", errno);
    return;
  }

  channum = 0; /*start at chan 1 */
  prevchannum = -1;
  while ( 1 ) {
    memset(rxbuf, 0, sizeof(rxbuf));

    if ( ioctl(fd, AD78XX_PERIOD_READ, &rxbuf[0]) ) {
      printf("Failed to read in periodic mode for AD78xx: %d\n", errno);
      return;
    }

    for (i=0; i<percfg.channel_cnt; i++) {
  		if ( prevchannum == 0 )
	  		printf("\n");
  		printf("%d: [0x%04x](0x%x : %d)\n", 
  			prevchannum,
  			rxbuf[i],
  		  (rxbuf[i] & 0xfff),
  			(rxbuf[i] & 0xfff));
    	prevchannum = channum;
	  	channum++;
		  if ( channum >= 8 ) {
			  channum = 0;
  		}
    }
  }

  if ( ioctl(fd, AD78XX_PERIOD_STOP, NULL) ) {
    printf("Failed to stop periodic mode for AD78xx: %d\n", errno);
  }
}
