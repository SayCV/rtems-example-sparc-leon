#ifndef __SPI_AD78XX_H__
#define __SPI_AD78XX_H__

#include <rtems/libi2c.h>

#define AD7891_CTRL_A2_BIT 7
#define AD7891_CTRL_A1_BIT 6
#define AD7891_CTRL_A0_BIT 5
#define AD7891_CTRL_CONV_BIT 4
#define AD7891_CTRL_STBY_BIT 3
#define AD7891_CTRL_FORMAT_BIT 2

#define AD7891_CTRL_A_MASK        (0x7<<AD7891_CTRL_A0_BIT)
#define AD7891_CTRL_CONV_MASK     (1<<AD7891_CTRL_CONV_BIT)
#define AD7891_CTRL_STBY_MASK     (1<<AD7891_CTRL_STBY_BIT)
#define AD7891_CTRL_FORMAT_MASK   (1<<AD7891_CTRL_FORMAT_BIT)

#define AD78XX_CONFIG_NEXT_CHAN 1
#define AD78XX_PERIOD_START 2
#define AD78XX_PERIOD_READ 3
#define AD78XX_PERIOD_STOP 4


struct spi_ad78xx_params {
  uint32_t    baudrate;         /* tfr rate, bits per second     */
  int         device_id;        /* Set to 7891 */
  int         channels;         /* Number of channels */
  int         channel_bits;     /* Number of bit in address */
  int         abits;            /* Number of ADC bits */
  
  /* Set to zero, internal to driver only */
  int         channel;          /* Select Channel */
  int         periodic_mode;
  int         periodic_channel_cnt;
  int         periodic_channles[8];
  int         periodic_started;
	int         periodic_exttrig;
};

struct spi_ad78xx_periodic_cfg {
  int   enable_periodic;
	int   periodic_exttrig; /* bit0: ExternalActivation. bit1:ExternalRepeat */
  int   channel_cnt;
  int   channels[8];
};

typedef struct {
  rtems_libi2c_drv_t         libi2c_drv_entry;  /* general i2c/spi params */
  struct spi_ad78xx_params   drv_params;        /* private parameters     */
} spi_ad78xx_drv_t;

extern rtems_driver_address_table spi_ad78xx_ro_ops;

#endif
