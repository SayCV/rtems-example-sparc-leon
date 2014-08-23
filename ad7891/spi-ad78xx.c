/* AD7891 ADC SPI driver, uses the SPICTRL driver interface to set up period
 * transfers to the AD7891, with periodic transfers the channels can be
 * read automatically by the hardware, when reading the periodic FIFO is
 * read.
 *
 * Copyright (C),
 * Aeroflex Gaisler 2009,
 *
 */
 
#include <rtems.h>
#include <bsp.h> /* for device driver prototypes */

#include <spictrl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>

#include <rtems.h>
#include <rtems/libio.h>

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "spi-ad78xx.h"

rtems_status_code spi_ad78xx_minor2param_ptr
(
 rtems_device_minor_number minor,                /* minor number of device */
 struct spi_ad78xx_params **params_ptr           /* ptr to param ptr       */
)
{
  rtems_status_code   rc = RTEMS_SUCCESSFUL;
  spi_ad78xx_drv_t *drv_ptr;

  if (rc == RTEMS_SUCCESSFUL) {
    rc = -rtems_libi2c_ioctl(minor,
			     RTEMS_LIBI2C_IOCTL_GET_DRV_T,
			     &drv_ptr);
  }
  if ((rc == RTEMS_SUCCESSFUL) &&
      (drv_ptr->libi2c_drv_entry.size != sizeof(spi_ad78xx_drv_t))) {
    rc = RTEMS_INVALID_SIZE;
  }
  if (rc == RTEMS_SUCCESSFUL) {
    *params_ptr = &(drv_ptr->drv_params);
  }
  return rc;
}

rtems_status_code spi_ad78xx_periodic_start
(
 rtems_device_minor_number minor,        /* minor device number            */
 struct spi_ad78xx_params *params
)
{
  rtems_status_code rc = RTEMS_SUCCESSFUL;
  unsigned short         cmdbuf[8];
  int                   ret_cnt = 0;
  int                   cmd_size, chan;

  rtems_libi2c_tfr_mode_t tfr_mode = {
  baudrate:      20000000, /* maximum bits per second                   */
  bits_per_char: 16,        /* how many bits per byte/word/longword?     */
  lsb_first:      FALSE,   /* FALSE: send MSB first                     */
  clock_inv:      FALSE,   /* FALSE: non-inverted clock (high active)   */
  clock_phs:      TRUE     /* FALSE: clock starts in middle of data tfr */
  };

  struct spictrl_ioctl_config pconfig;
  int len;
  struct spictrl_period_io period_io;

  /*
   * get mem parameters
   */
  if (rc == RTEMS_SUCCESSFUL) {
    rc = spi_ad78xx_minor2param_ptr(minor, &params);
  }

  /*
   * select device, set transfer mode, address device
   */
  if (rc == RTEMS_SUCCESSFUL) {
    rc = rtems_libi2c_send_start(minor);
  }
  /*
   * set transfer mode
   */
  if (rc == RTEMS_SUCCESSFUL) {
    tfr_mode.baudrate = params->baudrate;
    rc = -rtems_libi2c_ioctl(minor,
			     RTEMS_LIBI2C_IOCTL_SET_TFRMODE,
			     &tfr_mode);
  }

  pconfig.clock_gap = 31;
  pconfig.flags = SPICTRL_FLAGS_TAC;
  pconfig.periodic_mode = 1;
  /* 50 times a second @ 20MHz */
  pconfig.period = 1000;
  pconfig.period_flags = SPICTRL_PERIOD_FLAGS_STRICT | SPICTRL_PERIOD_FLAGS_ASEL;
  if ( params->periodic_exttrig & 0x1 ) {
    /* External trigger starts (activates) first transfer */
    pconfig.period_flags |= SPICTRL_PERIOD_FLAGS_EACT;
  }
  if ( params->periodic_exttrig & 0x2 ) {
    /* External trigger start each period (external repeat) */
    pconfig.period_flags |= SPICTRL_PERIOD_FLAGS_ERPT;
  }
  pconfig.period_slvsel = 0x7;
  rc = rtems_libi2c_ioctl(minor, SPICTRL_IOCTL_CONFIG, &pconfig);
  if ( rc != RTEMS_SUCCESSFUL ) {
    printk("FAILED configuring\n");
    exit(0);
  }

  /*
   * address device
   */
  if (rc == RTEMS_SUCCESSFUL) {
    rc = rtems_libi2c_send_addr(minor, TRUE);
  }

  /* Prepare periodic transmit registers / mask registers */
  period_io.masks[0] = 0;
  period_io.masks[1] = 0;
  period_io.masks[2] = 0;
  period_io.masks[3] = 0;
	period_io.options = 0x3;
  period_io.data = &cmdbuf[0]; /* Data that will be stored into TX regs */

  /*
   * send Convert channel command and address, then read the result.
   */
  for (chan=0; chan<params->periodic_channel_cnt; chan++) {
    cmdbuf[chan] = (((params->periodic_channles[chan] << AD7891_CTRL_A0_BIT) & AD7891_CTRL_A_MASK) |
              AD7891_CTRL_CONV_MASK) << 8;

    /* Enable TX Register by setting corresponding mask */
    period_io.masks[chan/32] |= 1 << (chan & 0x1f);
  }

  rc = rtems_libi2c_ioctl(minor, SPICTRL_IOCTL_PERIOD_WRITE, &period_io);
  if ( rc != 0 ) {
    printk("AD78XX: Failed writing TXREGS/MASKS(%d) in periodic mode: %d\n",
              cmd_size, rc);
  }

  /* Start Periodic transfers */
  rc = rtems_libi2c_ioctl(minor, SPICTRL_IOCTL_PERIOD_START, 0);
  if ( rc != RTEMS_SUCCESSFUL ) {
    printk("FAILED starting periodic\n");
    exit(0);
  }

  return 0;
}

rtems_status_code spi_ad78xx_periodic_read
(
 rtems_device_minor_number minor,        /* minor device number            */
 struct spi_ad78xx_params *params,
 unsigned short *buf
)
{
  int channel_cnt = params->periodic_channel_cnt;
  int i, rc;
  struct spictrl_period_io period_io;

  period_io.masks[0] = 0;
  period_io.masks[1] = 0;
  period_io.masks[2] = 0;
  period_io.masks[3] = 0;
  period_io.options = 0x2; /* Read out data from receive registers */
  period_io.data = buf;

  /* Clear buffer first and tell driver what registers to read */
  for (i=0; i<channel_cnt; i++) {
    buf[i] = 0;
    period_io.masks[i/32] |= 1 << (i & 0x1f);
  }

  /*
   * fetch read data 
   */
  rc = rtems_libi2c_ioctl(minor, SPICTRL_IOCTL_PERIOD_READ, &period_io);
  if ( rc != 0 ) {
    printk("AD78XX: Failed reading RXREGS/MASKS in periodic mode: %d\n", rc);
    return -1;
  }

  /* Number of bytes read */
  return params->periodic_channel_cnt * 2;
}

rtems_status_code spi_ad78xx_periodic_stop
(
 rtems_device_minor_number minor,        /* minor device number            */
 struct spi_ad78xx_params *params
)
{
  int rc;

  /* Stop Periodic transfers */
  rc = rtems_libi2c_ioctl(minor, SPICTRL_IOCTL_PERIOD_STOP, 0);
  if ( rc != RTEMS_SUCCESSFUL ) {
    printk("FAILED stopping periodic\n");
    return -1;
  }

  /*
   * terminate transfer
   */
  rc = rtems_libi2c_send_stop(minor);
  if ( rc != RTEMS_SUCCESSFUL )
    return -1;

  return 0;
}

rtems_status_code spi_ad78xx_read
(
 rtems_device_major_number major,        /* major device number            */
 rtems_device_minor_number minor,        /* minor device number            */
 void                      *arg          /* ptr to read argument struct    */
)
{
  rtems_status_code rc = RTEMS_SUCCESSFUL;
  rtems_libio_rw_args_t *rwargs = arg;
  int                       cnt = rwargs->count;
  unsigned char            *buf = (unsigned char *)rwargs->buffer;
  unsigned short         cmdbuf[1], result[4];
  int                   ret_cnt = 0;
  int                  cmd_size;
  struct spi_ad78xx_params  *params;

  rtems_libi2c_tfr_mode_t tfr_mode = {
  baudrate:       20000000, /* maximum bits per second                   */
  bits_per_char:  16,       /* how many bits per byte/word/longword?     */
  lsb_first:      FALSE,    /* FALSE: send MSB first                     */
  clock_inv:      FALSE,    /* FALSE: non-inverted clock (high active)   */
  clock_phs:      TRUE      /* FALSE: clock starts in middle of data tfr */
  };

  struct spictrl_ioctl_config pconfig;
  int times;

  /*
   * get mem parameters
   */
  if (rc == RTEMS_SUCCESSFUL) {
    rc = spi_ad78xx_minor2param_ptr(minor, &params);
  }
  /*
   * check arguments
   */
  if (rc == RTEMS_SUCCESSFUL) {

  }
  /*
   * select device, set transfer mode, address device
   */
  if (rc == RTEMS_SUCCESSFUL) {
    rc = rtems_libi2c_send_start(minor);
  }
  /*
   * set transfer mode
   */
  if (rc == RTEMS_SUCCESSFUL) {
    tfr_mode.baudrate = params->baudrate;
    rc = -rtems_libi2c_ioctl(minor,
			     RTEMS_LIBI2C_IOCTL_SET_TFRMODE,
			     &tfr_mode);
  }

  /*
   * address device
   */
  if (rc == RTEMS_SUCCESSFUL) {
    rc = rtems_libi2c_send_addr(minor, TRUE);
  }

  /*
   * send Convert channel command and address, then read the result.
   */
  cmdbuf[0] = (((params->channel << AD7891_CTRL_A0_BIT) & AD7891_CTRL_A_MASK) | 
              AD7891_CTRL_CONV_MASK) << 8;
  cmd_size = 2;
  {
    rtems_libi2c_read_write_t rwbuf;
    int len;

    rwbuf.byte_cnt = 2;
    rwbuf.rd_buf = (char *)&result[0];
    rwbuf.wr_buf = (char *)&cmdbuf[0];
    result[0] = 0;
    result[1] = 0;
		ret_cnt = rtems_libi2c_ioctl(minor, RTEMS_LIBI2C_IOCTL_READ_WRITE, &rwbuf);
    if ( ret_cnt != 2 ) {
      printf("IOCTL: READ/WRITE Failed: %d\n", ret_cnt);
    }
    buf[0] = (result[0] >> 8) & 0xff;
    buf[1] = result[0] & 0xff;
  }

  /*
   * terminate transfer
   */
  if (rc == RTEMS_SUCCESSFUL) {
    rc = rtems_libi2c_send_stop(minor);
  }
  rwargs->bytes_moved = (rc == RTEMS_SUCCESSFUL) ? ret_cnt : 0;

  return rc;
}

rtems_status_code spi_ad78xx_write
(
 rtems_device_major_number major,        /* major device number            */
 rtems_device_minor_number minor,        /* minor device number            */
 void                      *arg          /* ptr to write argument struct   */
)
{
  return RTEMS_NOT_DEFINED;
}

rtems_status_code spi_ad78xx_ioctl
(
 rtems_device_major_number major,        /* major device number            */
 rtems_device_minor_number minor,        /* minor device number            */
 void                      *arg          /* ptr to write argument struct   */
)
{
  rtems_libio_ioctl_args_t *ioarg = (rtems_libio_ioctl_args_t *)arg;
  struct spi_ad78xx_params *params;
  int rc;

  /*
   * get mem parameters
   */
  rc = spi_ad78xx_minor2param_ptr(minor, &params);
  if (rc != RTEMS_SUCCESSFUL) {
        return RTEMS_INVALID_NAME;
  }

  ioarg->ioctl_return = 0;
  switch ( ioarg->command ) {
    /* Select "next" channel to read from in normal mode.
     * When reading the ADC value, the next channel is decided
     * on beforehand.
     */
    case AD78XX_CONFIG_NEXT_CHAN: 
    {
      int channel = (int)ioarg->buffer;
      if ( (channel < 0) || (channel > params->channels) )
        return RTEMS_INVALID_NAME;
      params->channel = channel;
      break;
    }
    case AD78XX_PERIOD_START:  /* Select channels  to read from in periodic mode */
    {
      struct spi_ad78xx_periodic_cfg *cfg = (void *)ioarg->buffer;
      int chan;

      if ( params->periodic_started )
        return RTEMS_RESOURCE_IN_USE;

      /* Configure periodic mode */
      params->periodic_mode = cfg->enable_periodic;
      params->periodic_channel_cnt = cfg->channel_cnt;
      for ( chan=0; chan < cfg->channel_cnt; chan++) {
        params->periodic_channles[chan] = cfg->channels[chan];
      }
      params->periodic_exttrig = cfg->periodic_exttrig;

      /* Start periodic mode */
      if ( params->periodic_mode && spi_ad78xx_periodic_start(minor, params) ) 
        return RTEMS_IO_ERROR;
      params->periodic_started = 1;
      break;
    }
    case AD78XX_PERIOD_READ:
    {
      if ( !ioarg->buffer )
        return RTEMS_INVALID_NAME;

      if ( spi_ad78xx_periodic_read(minor, params, ioarg->buffer) < 0 ) {
        return RTEMS_IO_ERROR;
      }
      break;
    }
    case AD78XX_PERIOD_STOP:
    {
      spi_ad78xx_periodic_stop(minor, params);
      params->periodic_started = 0;
      break;
    }
    default:
      return RTEMS_NOT_DEFINED;
  }

  return RTEMS_SUCCESSFUL;
}


rtems_driver_address_table spi_ad78xx_ro_ops = {
  read_entry:     spi_ad78xx_read,
  control_entry:  spi_ad78xx_ioctl,
  write_entry:    spi_ad78xx_write,
};

/* Example definition */
#if 0

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
#endif
