/*
 * A RTEMS sample application using the GRPWM driver
 */

#include <rtems.h>

/* configuration information */

#define CONFIGURE_INIT

#include <bsp.h> /* for device driver prototypes */

rtems_task Init( rtems_task_argument argument);	/* forward declaration needed */

/* configuration information */
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
#define CONFIGURE_DRIVER_AMBAPP_GAISLER_GRPWM  /* GRPWM driver */

#ifdef LEON2
  /* AMBA PnP Support for GRLIB-LEON2 */
  #define CONFIGURE_DRIVER_LEON2_AMBAPP
#endif

#include <drvmgr/drvmgr_confdefs.h>

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <math.h>

#undef ENABLE_NETWORK
#undef ENABLE_NETWORK_SMC_LEON3

#include "config.c"
#include <grpwm.h>

/* Configure PWM Core frequency */
#define FREQ_HZ 40000000

int pwm_test(char *path);

rtems_task Init(
  rtems_task_argument ignored
)
{
	int status;

	/* Initialize Driver manager and Networking, in config.c */
	system_init();

	/* Print device topology */	
	rtems_drvmgr_print_topo();

	if ( (status=pwm_test("/dev/grpwm0")) != 0 ) {
		printf("PWM Test Failed (%d)\n", status);
		exit(-1);
	}

	exit( 0 );
}

/* 10MHz, 5MHz, 2.5MHz, 1MHz, 500kHz, 250kHz, 100kHz, 50Khz */
unsigned int scaler_freqs[8] = 
{
	10000000,
	5000000,
	2500000,
	1000000,
	500000,
	250000,
	100000,
	50000,
};

int pwm_config_channel(
	int fd,
	int channel,
	unsigned int cfg_irq,
	struct grpwm_ioctl_config *cfg
	)
{
	unsigned int irq = (channel<<8) | GRPWM_IRQ_DISABLE;
	int status;

	cfg->channel = channel;
	
	status = ioctl(fd, GRPWM_IOCTL_IRQ, irq);
	if ( status < 0 ) {
		printf("Failed to Disable IRQ on channel %d: %d (%s)\n", channel, errno, strerror(errno));
		return -1;
	}

	status = ioctl(fd, GRPWM_IOCTL_SET_CONFIG, cfg);
	if ( status < 0 ) {
		printf("Failed to Configure channel %d: %d (%s)\n", channel, errno, strerror(errno));
		return -1;
	}

	return 0;
}

int pwm_update_channel(
	int fd,
	int channel,
	struct grpwm_ioctl_update_chan *chan
	)
{
	int status;
	struct grpwm_ioctl_update up;

	up.chanmask = 1<<channel;
	up.channels[channel] = *chan;

	status = ioctl(fd, GRPWM_IOCTL_UPDATE, &up);
	if ( status < 0 ) {
		printf("Failed to update channel %d: %d (%s)\n", channel, errno, strerror(errno));
		return -1;
	}
	return 0;
}

void pwm_isr(int channel, void *arg)
{
	printk("PWM_ISR: channel %d, arg: %x\n", channel, arg);
}

unsigned int wave_ram_content32[360] =
{
 /* 0000 */     0x7fffffff, 0x823be164, 0x84779631, 0x86b2f1d1,
 /* 0004 */     0x88edc7b6, 0x8b27eb5b, 0x8d61304d, 0x8f996a25,
 /* 0008 */     0x91d06c95, 0x94060b67, 0x963a1a7d, 0x986c6ddc,
 /* 000c */     0x9a9cd9ab, 0x9ccb3236, 0x9ef74bf2, 0xa120fb82,
 /* 0010 */     0xa34815b9, 0xa56c6f9e, 0xa78dde6d, 0xa9ac379f,
 /* 0014 */     0xabc750e8, 0xaddf003e, 0xaff31bdd, 0xb2037a44,
 /* 0018 */     0xb40ff241, 0xb6185aed, 0xb81c8bb4, 0xba1c5c55,
 /* 001c */     0xbc17a4e7, 0xbe0e3ddb, 0xbfffffff, 0xc1ecc483,
 /* 0020 */     0xc3d464f9, 0xc5b6bb5c, 0xc793a20f, 0xc96af3e1,
 /* 0024 */     0xcb3c8c11, 0xcd084650, 0xcecdfec6, 0xd08d9210,
 /* 0028 */     0xd246dd48, 0xd3f9be04, 0xd5a6125a, 0xd74bb8e5,
 /* 002c */     0xd8ea90c2, 0xda827999, 0xdc135399, 0xdd9cff82,
 /* 0030 */     0xdf1f5e9f, 0xe09a52d1, 0xe20dbe8a, 0xe37984d3,
 /* 0034 */     0xe4dd894e, 0xe639b039, 0xe78dde6d, 0xe8d9f963,
 /* 0038 */     0xea1de735, 0xeb598ea1, 0xec8cd70a, 0xedb7a878,
 /* 003c */     0xeed9eba0, 0xeff389de, 0xf1046d3c, 0xf20c8074,
 /* 0040 */     0xf30baeec, 0xf401e4bf, 0xf4ef0ebb, 0xf5d31a5f,
 /* 0044 */     0xf6adf5e5, 0xf77f903b, 0xf847d908, 0xf906c0af,
 /* 0048 */     0xf9bc384c, 0xfa6831b8, 0xfb0a9f8c, 0xfba3751c,
 /* 004c */     0xfc32a67c, 0xfcb82884, 0xfd33f0c8, 0xfda5f5a3,
 /* 0050 */     0xfe0e2e31, 0xfe6c924f, 0xfec11aa3, 0xff0bc095,
 /* 0054 */     0xff4c7e52, 0xff834ecf, 0xffb02dc4, 0xffd317b3,
 /* 0058 */     0xffec09e1, 0xfffb025e, 0xffffffff, 0xfffb025e,
 /* 005c */     0xffec09e1, 0xffd317b3, 0xffb02dc4, 0xff834ecf,
 /* 0060 */     0xff4c7e52, 0xff0bc095, 0xfec11aa3, 0xfe6c924f,
 /* 0064 */     0xfe0e2e31, 0xfda5f5a3, 0xfd33f0c8, 0xfcb82884,
 /* 0068 */     0xfc32a67c, 0xfba3751c, 0xfb0a9f8c, 0xfa6831b8,
 /* 006c */     0xf9bc384c, 0xf906c0af, 0xf847d908, 0xf77f903b,
 /* 0070 */     0xf6adf5e5, 0xf5d31a5f, 0xf4ef0ebb, 0xf401e4bf,
 /* 0074 */     0xf30baeec, 0xf20c8074, 0xf1046d3c, 0xeff389de,
 /* 0078 */     0xeed9eba0, 0xedb7a878, 0xec8cd70a, 0xeb598ea1,
 /* 007c */     0xea1de735, 0xe8d9f963, 0xe78dde6d, 0xe639b039,
 /* 0080 */     0xe4dd894e, 0xe37984d3, 0xe20dbe8a, 0xe09a52d1,
 /* 0084 */     0xdf1f5e9f, 0xdd9cff82, 0xdc135399, 0xda827999,
 /* 0088 */     0xd8ea90c2, 0xd74bb8e5, 0xd5a6125a, 0xd3f9be04,
 /* 008c */     0xd246dd48, 0xd08d9210, 0xcecdfec6, 0xcd084650,
 /* 0090 */     0xcb3c8c11, 0xc96af3e1, 0xc793a20f, 0xc5b6bb5c,
 /* 0094 */     0xc3d464f9, 0xc1ecc483, 0xbfffffff, 0xbe0e3ddb,
 /* 0098 */     0xbc17a4e7, 0xba1c5c55, 0xb81c8bb4, 0xb6185aed,
 /* 009c */     0xb40ff241, 0xb2037a44, 0xaff31bdd, 0xaddf003e,
 /* 00a0 */     0xabc750e8, 0xa9ac379f, 0xa78dde6d, 0xa56c6f9e,
 /* 00a4 */     0xa34815b9, 0xa120fb82, 0x9ef74bf2, 0x9ccb3236,
 /* 00a8 */     0x9a9cd9ab, 0x986c6ddc, 0x963a1a7d, 0x94060b67,
 /* 00ac */     0x91d06c95, 0x8f996a25, 0x8d61304d, 0x8b27eb5b,
 /* 00b0 */     0x88edc7b6, 0x86b2f1d1, 0x84779631, 0x823be164,
 /* 00b4 */     0x7fffffff, 0x7dc41e9a, 0x7b8869cd, 0x794d0e2d,
 /* 00b8 */     0x77123848, 0x74d814a3, 0x729ecfb1, 0x706695d9,
 /* 00bc */     0x6e2f9369, 0x6bf9f497, 0x69c5e581, 0x67939222,
 /* 00c0 */     0x65632653, 0x6334cdc8, 0x6108b40c, 0x5edf047c,
 /* 00c4 */     0x5cb7ea45, 0x5a939060, 0x58722191, 0x5653c85f,
 /* 00c8 */     0x5438af16, 0x5220ffc0, 0x500ce421, 0x4dfc85ba,
 /* 00cc */     0x4bf00dbd, 0x49e7a511, 0x47e3744a, 0x45e3a3a9,
 /* 00d0 */     0x43e85b17, 0x41f1c223, 0x3fffffff, 0x3e133b7b,
 /* 00d4 */     0x3c2b9b05, 0x3a4944a2, 0x386c5def, 0x36950c1d,
 /* 00d8 */     0x34c373ed, 0x32f7b9ae, 0x31320138, 0x2f726dee,
 /* 00dc */     0x2db922b6, 0x2c0641fa, 0x2a59eda4, 0x28b44719,
 /* 00e0 */     0x27156f3c, 0x257d8665, 0x23ecac65, 0x2263007c,
 /* 00e4 */     0x20e0a15f, 0x1f65ad2d, 0x1df24174, 0x1c867b2b,
 /* 00e8 */     0x1b2276b0, 0x19c64fc5, 0x18722191, 0x1726069b,
 /* 00ec */     0x15e218c9, 0x14a6715d, 0x137328f4, 0x12485786,
 /* 00f0 */     0x1126145e, 0x100c7620, 0x0efb92c2, 0x0df37f8a,
 /* 00f4 */     0x0cf45112, 0x0bfe1b3f, 0x0b10f143, 0x0a2ce59f,
 /* 00f8 */     0x09520a19, 0x08806fc3, 0x07b826f6, 0x06f93f4f,
 /* 00fc */     0x0643c7b2, 0x0597ce46, 0x04f56072, 0x045c8ae2,
 /* 0100 */     0x03cd5982, 0x0347d77a, 0x02cc0f36, 0x025a0a5b,
 /* 0104 */     0x01f1d1cd, 0x01936daf, 0x013ee55b, 0x00f43f69,
 /* 0108 */     0x00b381ac, 0x007cb12f, 0x004fd23a, 0x002ce84b,
 /* 010c */     0x0013f61d, 0x0004fda0, 0x00000000, 0x0004fda0,
 /* 0110 */     0x0013f61d, 0x002ce84b, 0x004fd23a, 0x007cb12f,
 /* 0114 */     0x00b381ac, 0x00f43f69, 0x013ee55b, 0x01936daf,
 /* 0118 */     0x01f1d1cd, 0x025a0a5b, 0x02cc0f36, 0x0347d77a,
 /* 011c */     0x03cd5982, 0x045c8ae2, 0x04f56072, 0x0597ce46,
 /* 0120 */     0x0643c7b2, 0x06f93f4f, 0x07b826f6, 0x08806fc3,
 /* 0124 */     0x09520a19, 0x0a2ce59f, 0x0b10f143, 0x0bfe1b3f,
 /* 0128 */     0x0cf45112, 0x0df37f8a, 0x0efb92c2, 0x100c7620,
 /* 012c */     0x1126145e, 0x12485786, 0x137328f4, 0x14a6715d,
 /* 0130 */     0x15e218c9, 0x1726069b, 0x18722191, 0x19c64fc5,
 /* 0134 */     0x1b2276b0, 0x1c867b2b, 0x1df24174, 0x1f65ad2d,
 /* 0138 */     0x20e0a15f, 0x2263007c, 0x23ecac65, 0x257d8665,
 /* 013c */     0x27156f3c, 0x28b44719, 0x2a59eda4, 0x2c0641fa,
 /* 0140 */     0x2db922b6, 0x2f726dee, 0x31320138, 0x32f7b9ae,
 /* 0144 */     0x34c373ed, 0x36950c1d, 0x386c5def, 0x3a4944a2,
 /* 0148 */     0x3c2b9b05, 0x3e133b7b, 0x3fffffff, 0x41f1c223,
 /* 014c */     0x43e85b17, 0x45e3a3a9, 0x47e3744a, 0x49e7a511,
 /* 0150 */     0x4bf00dbd, 0x4dfc85ba, 0x500ce421, 0x5220ffc0,
 /* 0154 */     0x5438af16, 0x5653c85f, 0x58722191, 0x5a939060,
 /* 0158 */     0x5cb7ea45, 0x5edf047c, 0x6108b40c, 0x6334cdc8,
 /* 015c */     0x65632653, 0x67939222, 0x69c5e581, 0x6bf9f497,
 /* 0160 */     0x6e2f9369, 0x706695d9, 0x729ecfb1, 0x74d814a3,
 /* 0164 */     0x77123848, 0x794d0e2d, 0x7b8869cd, 0x7dc41e9a
};

unsigned int wave_ram_content8[360] =
{
 /* 0000 */     0x7f, 0x81, 0x83, 0x86,
 /* 0004 */     0x88, 0x8a, 0x8c, 0x8f,
 /* 0008 */     0x91, 0x93, 0x95, 0x97,
 /* 000c */     0x9a, 0x9c, 0x9e, 0xa0,
 /* 0010 */     0xa2, 0xa4, 0xa6, 0xa9,
 /* 0014 */     0xab, 0xad, 0xaf, 0xb1,
 /* 0018 */     0xb3, 0xb5, 0xb7, 0xb9,
 /* 001c */     0xbb, 0xbd, 0xbf, 0xc1,
 /* 0020 */     0xc3, 0xc4, 0xc6, 0xc8,
 /* 0024 */     0xca, 0xcc, 0xcd, 0xcf,
 /* 0028 */     0xd1, 0xd3, 0xd4, 0xd6,
 /* 002c */     0xd8, 0xd9, 0xdb, 0xdc,
 /* 0030 */     0xde, 0xdf, 0xe1, 0xe2,
 /* 0034 */     0xe3, 0xe5, 0xe6, 0xe7,
 /* 0038 */     0xe9, 0xea, 0xeb, 0xec,
 /* 003c */     0xed, 0xef, 0xf0, 0xf1,
 /* 0040 */     0xf2, 0xf3, 0xf3, 0xf4,
 /* 0044 */     0xf5, 0xf6, 0xf7, 0xf8,
 /* 0048 */     0xf8, 0xf9, 0xfa, 0xfa,
 /* 004c */     0xfb, 0xfb, 0xfc, 0xfc,
 /* 0050 */     0xfd, 0xfd, 0xfd, 0xfe,
 /* 0054 */     0xfe, 0xfe, 0xfe, 0xfe,
 /* 0058 */     0xfe, 0xfe, 0xff, 0xfe,
 /* 005c */     0xfe, 0xfe, 0xfe, 0xfe,
 /* 0060 */     0xfe, 0xfe, 0xfd, 0xfd,
 /* 0064 */     0xfd, 0xfc, 0xfc, 0xfb,
 /* 0068 */     0xfb, 0xfa, 0xfa, 0xf9,
 /* 006c */     0xf8, 0xf8, 0xf7, 0xf6,
 /* 0070 */     0xf5, 0xf4, 0xf3, 0xf3,
 /* 0074 */     0xf2, 0xf1, 0xf0, 0xef,
 /* 0078 */     0xed, 0xec, 0xeb, 0xea,
 /* 007c */     0xe9, 0xe7, 0xe6, 0xe5,
 /* 0080 */     0xe3, 0xe2, 0xe1, 0xdf,
 /* 0084 */     0xde, 0xdc, 0xdb, 0xd9,
 /* 0088 */     0xd8, 0xd6, 0xd4, 0xd3,
 /* 008c */     0xd1, 0xcf, 0xcd, 0xcc,
 /* 0090 */     0xca, 0xc8, 0xc6, 0xc4,
 /* 0094 */     0xc3, 0xc1, 0xbf, 0xbd,
 /* 0098 */     0xbb, 0xb9, 0xb7, 0xb5,
 /* 009c */     0xb3, 0xb1, 0xaf, 0xad,
 /* 00a0 */     0xab, 0xa9, 0xa6, 0xa4,
 /* 00a4 */     0xa2, 0xa0, 0x9e, 0x9c,
 /* 00a8 */     0x9a, 0x97, 0x95, 0x93,
 /* 00ac */     0x91, 0x8f, 0x8c, 0x8a,
 /* 00b0 */     0x88, 0x86, 0x83, 0x81,
 /* 00b4 */     0x7f, 0x7d, 0x7b, 0x78,
 /* 00b8 */     0x76, 0x74, 0x72, 0x6f,
 /* 00bc */     0x6d, 0x6b, 0x69, 0x67,
 /* 00c0 */     0x64, 0x62, 0x60, 0x5e,
 /* 00c4 */     0x5c, 0x5a, 0x58, 0x55,
 /* 00c8 */     0x53, 0x51, 0x4f, 0x4d,
 /* 00cc */     0x4b, 0x49, 0x47, 0x45,
 /* 00d0 */     0x43, 0x41, 0x3f, 0x3d,
 /* 00d4 */     0x3b, 0x3a, 0x38, 0x36,
 /* 00d8 */     0x34, 0x32, 0x31, 0x2f,
 /* 00dc */     0x2d, 0x2b, 0x2a, 0x28,
 /* 00e0 */     0x26, 0x25, 0x23, 0x22,
 /* 00e4 */     0x20, 0x1f, 0x1d, 0x1c,
 /* 00e8 */     0x1b, 0x19, 0x18, 0x17,
 /* 00ec */     0x15, 0x14, 0x13, 0x12,
 /* 00f0 */     0x11, 0x0f, 0x0e, 0x0d,
 /* 00f4 */     0x0c, 0x0b, 0x0b, 0x0a,
 /* 00f8 */     0x09, 0x08, 0x07, 0x06,
 /* 00fc */     0x06, 0x05, 0x04, 0x04,
 /* 0100 */     0x03, 0x03, 0x02, 0x02,
 /* 0104 */     0x01, 0x01, 0x01, 0x00,
 /* 0108 */     0x00, 0x00, 0x00, 0x00,
 /* 010c */     0x00, 0x00, 0x00, 0x00,
 /* 0110 */     0x00, 0x00, 0x00, 0x00,
 /* 0114 */     0x00, 0x00, 0x01, 0x01,
 /* 0118 */     0x01, 0x02, 0x02, 0x03,
 /* 011c */     0x03, 0x04, 0x04, 0x05,
 /* 0120 */     0x06, 0x06, 0x07, 0x08,
 /* 0124 */     0x09, 0x0a, 0x0b, 0x0b,
 /* 0128 */     0x0c, 0x0d, 0x0e, 0x0f,
 /* 012c */     0x11, 0x12, 0x13, 0x14,
 /* 0130 */     0x15, 0x17, 0x18, 0x19,
 /* 0134 */     0x1b, 0x1c, 0x1d, 0x1f,
 /* 0138 */     0x20, 0x22, 0x23, 0x25,
 /* 013c */     0x26, 0x28, 0x2a, 0x2b,
 /* 0140 */     0x2d, 0x2f, 0x31, 0x32,
 /* 0144 */     0x34, 0x36, 0x38, 0x3a,
 /* 0148 */     0x3b, 0x3d, 0x3f, 0x41,
 /* 014c */     0x43, 0x45, 0x47, 0x49,
 /* 0150 */     0x4b, 0x4d, 0x4f, 0x51,
 /* 0154 */     0x53, 0x55, 0x58, 0x5a,
 /* 0158 */     0x5c, 0x5e, 0x60, 0x62,
 /* 015c */     0x64, 0x67, 0x69, 0x6b,
 /* 0160 */     0x6d, 0x6f, 0x72, 0x74,
 /* 0164 */     0x76, 0x78, 0x7b, 0x7d
};

#if 0
/* Removed every three samples from wave_ram_content8[360] above.
 * it is better for 256 bytes RAMs.
 */
unsigned int wave_ram_content8[240] =
{
0x81, 0x83, 0x88, 0x8a, 
0x8f, 0x91, 0x95, 0x97, 
0x9c, 0x9e, 0xa2, 0xa4, 
0xa9, 0xab, 0xaf, 0xb1, 
0xb5, 0xb7, 0xbb, 0xbd, 
0xc1, 0xc3, 0xc6, 0xc8, 
0xcc, 0xcd, 0xd1, 0xd3, 
0xd6, 0xd8, 0xdb, 0xdc, 
0xdf, 0xe1, 0xe3, 0xe5, 
0xe7, 0xe9, 0xeb, 0xec, 
0xef, 0xf0, 0xf2, 0xf3, 
0xf4, 0xf5, 0xf7, 0xf8, 
0xf9, 0xfa, 0xfb, 0xfb, 
0xfc, 0xfd, 0xfd, 0xfe, 
0xfe, 0xfe, 0xfe, 0xfe, 
0xfe, 0xfe, 0xfe, 0xfe, 
0xfe, 0xfd, 0xfd, 0xfc, 
0xfb, 0xfb, 0xfa, 0xf9, 
0xf8, 0xf7, 0xf5, 0xf4, 
0xf3, 0xf2, 0xf0, 0xef, 
0xec, 0xeb, 0xe9, 0xe7, 
0xe5, 0xe3, 0xe1, 0xdf, 
0xdc, 0xdb, 0xd8, 0xd6, 
0xd3, 0xd1, 0xcd, 0xcc, 
0xc8, 0xc6, 0xc3, 0xc1, 
0xbd, 0xbb, 0xb7, 0xb5, 
0xb1, 0xaf, 0xab, 0xa9, 
0xa4, 0xa2, 0x9e, 0x9c, 
0x97, 0x95, 0x91, 0x8f, 
0x8a, 0x88, 0x83, 0x81, 
0x7d, 0x7b, 0x76, 0x74, 
0x6f, 0x6d, 0x69, 0x67, 
0x62, 0x60, 0x5c, 0x5a, 
0x55, 0x53, 0x4f, 0x4d, 
0x49, 0x47, 0x43, 0x41, 
0x3d, 0x3b, 0x38, 0x36, 
0x32, 0x31, 0x2d, 0x2b, 
0x28, 0x26, 0x23, 0x22, 
0x1f, 0x1d, 0x1b, 0x19, 
0x17, 0x15, 0x13, 0x12, 
0x0f, 0x0e, 0x0c, 0x0b, 
0x0a, 0x09, 0x07, 0x06, 
0x05, 0x04, 0x03, 0x03, 
0x02, 0x01, 0x01, 0x00, 
0x00, 0x00, 0x00, 0x00, 
0x00, 0x00, 0x00, 0x00, 
0x00, 0x01, 0x01, 0x02, 
0x03, 0x03, 0x04, 0x05, 
0x06, 0x07, 0x09, 0x0a, 
0x0b, 0x0c, 0x0e, 0x0f, 
0x12, 0x13, 0x15, 0x17, 
0x19, 0x1b, 0x1d, 0x1f, 
0x22, 0x23, 0x26, 0x28, 
0x2b, 0x2d, 0x31, 0x32, 
0x36, 0x38, 0x3b, 0x3d, 
0x41, 0x43, 0x47, 0x49, 
0x4d, 0x4f, 0x53, 0x55, 
0x5a, 0x5c, 0x60, 0x62, 
0x67, 0x69, 0x6d, 0x6f, 
0x74, 0x76, 0x7b, 0x7d, 
};
#endif

#if 0
void init_wave_form_ram(unsigned int *location, unsigned int max)
{
	int i;
	double y, x, w, z;

	for(i=0; i<360; i++) {
		x = ((double)i * M_PI * 2.0) / 360.0;

		/*y = 2.0 * (3.14 * y) / 360.0;*/
		y = sin(x);

		/* remove negative part and normalize */
		w = (y + 1.0) / 2.0;

		/* Convert to integer, 0..1 => 0..0xffffffff (if 32-bit) */
		z = w * (double)(max);
		z = floor(z);

		location[i] = (unsigned int)z;
		printf("%d: sin(%e) = %e => %e ~= %e\n", i, x, y, w, z);
	}
}
#endif

/* Example how to use the GRPWM driver */
int pwm_test(char *path)
{
	int fd, status;
	struct grpwm_ioctl_cap cap;
	struct grpwm_ioctl_scaler scalers;
	int nscalers, i;
	struct grpwm_ioctl_config cfg;
	double x;
	struct grpwm_ioctl_update_chan val;

	fd = open(path, O_RDWR);
	if ( fd < 0 ) {
		printf("Failed to open GRPWM driver: %d (%s)\n", errno, strerror(errno));
		return -1;
	}

	status = ioctl(fd, GRPWM_IOCTL_GET_CAP, &cap);
	if ( status < 0 ) {
		printf("Failed to get capabilities: %d (%s)\n", errno, strerror(errno));
		return -2;
	}
	printf("NPWM:  %d\n", cap.channel_cnt);
	printf("PWM:   0x%08x\n", cap.pwm);
	printf("WAVE:  0x%08x\n", cap.wave);

	nscalers = ((cap.pwm >> 13) & 0x7) + 1;

	/* Configure scalers */
	scalers.index_mask = 0;
	for (i=0; i<nscalers; i++) {
		scalers.index_mask |= 1<<i;
		scalers.values[i] = (FREQ_HZ / scaler_freqs[i]) - 1;
		printf("Initing scaler %d to %dHz: %d\n", i, scaler_freqs[i], scalers.values[i]);
	}
	status = ioctl(fd, GRPWM_IOCTL_SET_SCALER, &scalers);
	if ( status < 0 ) {
		printf("Failed to set scalers: %d (%s)\n", errno, strerror(errno));
		return -3;
	}

	/*** Configure channel 1 ***/
	cfg.options = GRPWM_CONFIG_OPTION_SYMMETRIC | GRPWM_CONFIG_OPTION_PAIR | GRPWM_CONFIG_OPTION_POLARITY_HIGH;
	cfg.dbscaler = 0;
	cfg.scaler_index = 0;
	cfg.irqscaler = 0;
	cfg.isr = pwm_isr;
	cfg.isr_arg = 0;
	cfg.wave_activate = 0;
	cfg.wave_synccfg = 0;
	cfg.wave_sync = 0;
	cfg.wave_data_length = 0;
	cfg.wave_data = NULL;

	/* Disable IRQ and set config */
	if ( pwm_config_channel(fd, 0, GRPWM_IRQ_DISABLE, &cfg) ) {
		printf("Failed to configure Channel 0\n");
		return -4;
	}

	/*** Configure channel 2 ***/
	cfg.options = GRPWM_CONFIG_OPTION_ASYMMERTIC | GRPWM_CONFIG_OPTION_PAIR | GRPWM_CONFIG_OPTION_POLARITY_HIGH;
	cfg.dbscaler = 0;
	cfg.scaler_index = 0;
	cfg.irqscaler = 0;
	cfg.isr = pwm_isr;
	cfg.isr_arg = 0;
	cfg.wave_activate = 0;
	cfg.wave_synccfg = 0;
	cfg.wave_sync = 0;
	cfg.wave_data_length = 0;
	cfg.wave_data = NULL;

	/* Disable IRQ and set config */
	if ( pwm_config_channel(fd, 1, GRPWM_IRQ_DISABLE, &cfg) ) {
		printf("Failed to configure Channel 0\n");
		return -5;
	}

	/*** Configure channel 3 ***/
	cfg.options = GRPWM_CONFIG_OPTION_SYMMETRIC | GRPWM_CONFIG_OPTION_PAIR | GRPWM_CONFIG_OPTION_POLARITY_HIGH | GRPWM_CONFIG_OPTION_DEAD_BAND;
	cfg.dbscaler = 4-1; /* div 4, 40MHz->10MHz */
	cfg.scaler_index = 0;
	cfg.irqscaler = 0;
	cfg.isr = pwm_isr;
	cfg.isr_arg = 0;
	cfg.wave_activate = 0;
	cfg.wave_synccfg = 0;
	cfg.wave_sync = 0;
	cfg.wave_data_length = 0;
	cfg.wave_data = 0;

	/* Disable IRQ and set config */
	if ( pwm_config_channel(fd, 2, GRPWM_IRQ_DISABLE, &cfg) ) {
		printf("Failed to configure Channel 0\n");
		return -6;
	}


	/*** Configure channel 4 ***/
	/*GRPWM_CONFIG_OPTION_PAIR*/
	cfg.options = GRPWM_CONFIG_OPTION_POLARITY_HIGH;
	cfg.dbscaler = 0;
	cfg.scaler_index = 0;
	cfg.irqscaler = 0;
	cfg.isr = pwm_isr;
	cfg.isr_arg = 0;
	cfg.wave_activate = 1;
	cfg.wave_synccfg = 0; /* wsynccfg and wsen : (0x3<<30) | (1<<29) */
	cfg.wave_sync = 0; /* wsynccomp 0x1fff<<0 */
	cfg.wave_data_length = 360;
	cfg.wave_data = &wave_ram_content8[0];
#if 0
	pwm_init_wave_form_ram(&wave_ram_content8[0], 0xff);
	/*
	for(i=0; i<360; i++) {
		printf("%d: 0x%x (%d)\n", i, wave_ram_content[i], wave_ram_content[i]);
	}
	*/
#endif

	/* Disable IRQ and set config */
	if ( pwm_config_channel(fd, 3, GRPWM_IRQ_DISABLE, &cfg) ) {
		printf("Failed to configure Channel 0\n");
		return -7;
	}

	/* Enable channels */
	
	/*** Channel 1 ***/
	val.options =
		GRPWM_UPDATE_OPTION_ENABLE | 
		GRPWM_UPDATE_OPTION_PERIOD | 
		GRPWM_UPDATE_OPTION_COMP |
		GRPWM_UPDATE_OPTION_FIX;
	val.period = 0xff;
	val.compare = 0x40; /* % */
	val.dbcomp = 0;
	val.fix = GRPWM_UPDATE_FIX_DISABLE;
	pwm_update_channel(fd, 0, &val);

	/*** Channel 1 ***/
	val.options =
		GRPWM_UPDATE_OPTION_ENABLE | 
		GRPWM_UPDATE_OPTION_PERIOD | 
		GRPWM_UPDATE_OPTION_COMP |
		GRPWM_UPDATE_OPTION_FIX;
	val.period = 0xff;
	val.compare = 0x80; /* 50% for asymmetric  */
	val.dbcomp = 0;
	val.fix = GRPWM_UPDATE_FIX_DISABLE;
	pwm_update_channel(fd, 1, &val);

	/*** Channel 2 ***/
	val.options =
		GRPWM_UPDATE_OPTION_ENABLE | 
		GRPWM_UPDATE_OPTION_PERIOD | 
		GRPWM_UPDATE_OPTION_COMP |
		GRPWM_UPDATE_OPTION_DBCOMP |
		GRPWM_UPDATE_OPTION_FIX;
	val.period = 0xff;
	val.compare = 0x40; /* 25% for symmetric  */
	val.dbcomp = 10;
	val.fix = GRPWM_UPDATE_FIX_DISABLE;
	pwm_update_channel(fd, 2, &val);

	/*** Channel 3 ***/
	val.options =
		GRPWM_UPDATE_OPTION_ENABLE |
		GRPWM_UPDATE_OPTION_PERIOD |
		GRPWM_UPDATE_OPTION_COMP |
		GRPWM_UPDATE_OPTION_FIX;
	val.period = 0xff;
	val.compare = 0; /* taken from wave form RAM */
	val.dbcomp = 0;
	val.fix = GRPWM_UPDATE_FIX_DISABLE;
	pwm_update_channel(fd, 3, &val);

	return 0;
}
