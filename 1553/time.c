#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <stdio.h>

#include <rtems.h>

#include <spwcuc.h>
#include <grspw.h>

/********************** SPWCUC ***********************/

static unsigned char RMAP_CRCTable[256] = {
    0x00, 0x91, 0xe3, 0x72, 0x07, 0x96, 0xe4, 0x75,
    0x0e, 0x9f, 0xed, 0x7c, 0x09, 0x98, 0xea, 0x7b,
    0x1c, 0x8d, 0xff, 0x6e, 0x1b, 0x8a, 0xf8, 0x69,
    0x12, 0x83, 0xf1, 0x60, 0x15, 0x84, 0xf6, 0x67,
    0x38, 0xa9, 0xdb, 0x4a, 0x3f, 0xae, 0xdc, 0x4d,
    0x36, 0xa7, 0xd5, 0x44, 0x31, 0xa0, 0xd2, 0x43,
    0x24, 0xb5, 0xc7, 0x56, 0x23, 0xb2, 0xc0, 0x51,
    0x2a, 0xbb, 0xc9, 0x58, 0x2d, 0xbc, 0xce, 0x5f,
    0x70, 0xe1, 0x93, 0x02, 0x77, 0xe6, 0x94, 0x05,
    0x7e, 0xef, 0x9d, 0x0c, 0x79, 0xe8, 0x9a, 0x0b,
    0x6c, 0xfd, 0x8f, 0x1e, 0x6b, 0xfa, 0x88, 0x19,
    0x62, 0xf3, 0x81, 0x10, 0x65, 0xf4, 0x86, 0x17,
    0x48, 0xd9, 0xab, 0x3a, 0x4f, 0xde, 0xac, 0x3d,
    0x46, 0xd7, 0xa5, 0x34, 0x41, 0xd0, 0xa2, 0x33,
    0x54, 0xc5, 0xb7, 0x26, 0x53, 0xc2, 0xb0, 0x21,
    0x5a, 0xcb, 0xb9, 0x28, 0x5d, 0xcc, 0xbe, 0x2f,
    0xe0, 0x71, 0x03, 0x92, 0xe7, 0x76, 0x04, 0x95,
    0xee, 0x7f, 0x0d, 0x9c, 0xe9, 0x78, 0x0a, 0x9b,
    0xfc, 0x6d, 0x1f, 0x8e, 0xfb, 0x6a, 0x18, 0x89,
    0xf2, 0x63, 0x11, 0x80, 0xf5, 0x64, 0x16, 0x87,
    0xd8, 0x49, 0x3b, 0xaa, 0xdf, 0x4e, 0x3c, 0xad,
    0xd6, 0x47, 0x35, 0xa4, 0xd1, 0x40, 0x32, 0xa3,
    0xc4, 0x55, 0x27, 0xb6, 0xc3, 0x52, 0x20, 0xb1,
    0xca, 0x5b, 0x29, 0xb8, 0xcd, 0x5c, 0x2e, 0xbf,
    0x90, 0x01, 0x73, 0xe2, 0x97, 0x06, 0x74, 0xe5,
    0x9e, 0x0f, 0x7d, 0xec, 0x99, 0x08, 0x7a, 0xeb,
    0x8c, 0x1d, 0x6f, 0xfe, 0x8b, 0x1a, 0x68, 0xf9,
    0x82, 0x13, 0x61, 0xf0, 0x85, 0x14, 0x66, 0xf7,
    0xa8, 0x39, 0x4b, 0xda, 0xaf, 0x3e, 0x4c, 0xdd,
    0xa6, 0x37, 0x45, 0xd4, 0xa1, 0x30, 0x42, 0xd3,
    0xb4, 0x25, 0x57, 0xc6, 0xb3, 0x22, 0x50, 0xc1,
    0xba, 0x2b, 0x59, 0xc8, 0xbd, 0x2c, 0x5e, 0xcf,
};

static unsigned char calc_rmap_crc(unsigned char *data, unsigned int len)
{
    unsigned char crc = 0;
    unsigned int i;
    for (i = 0; i < len; i++) {
        crc = RMAP_CRCTable[(crc ^ data[i]) & 0xff];
    }
    return crc;
}

/* Create a SpaceWire CUCTP packet 
 *
 * Input: dla - destination logical address
 *        pid - protocol id
 *        tid - time code identification: 1 = 1958 January 1 (Level 1)
 *                                        2 = Agency defined (Level 2)
 *        init          - 1 = force initialization, 0 = synchronization
 *        cuctp_pkt_buf - user supplied buffer for storing packet, must hold 12 bytes
 *        regs          - struct spwcuc_regs pointer
 *
 * The function will read the Next ET register of the SpaceWire CUCTP handler and
 * create the CUCTP packet including CRC.
 *
 */
void spwcuc_create_cuctp_packet(
	void *spwcuc,
	unsigned char dla,
	unsigned char pid,
	unsigned char tid,
	unsigned char init, 
	unsigned char *cuctp_pkt_buf
	)
{
	/* This routine assumes long long is 64 bits and big endian architecture */
	unsigned long long et_next = spwcuc_get_next_et(spwcuc);
	unsigned char *p_et = (unsigned char *) &et_next;

	cuctp_pkt_buf[0] = dla;
	cuctp_pkt_buf[1] = pid;
	cuctp_pkt_buf[2] = 1<<7 | (tid&0x7)<<4 | 0xf; /* P-field default */
	cuctp_pkt_buf[3] = (init&1)<<7;               /* P-field extended */
	memcpy(&cuctp_pkt_buf[4], &p_et[1], 7);
	cuctp_pkt_buf[11] = calc_rmap_crc(cuctp_pkt_buf, 11);
}

int time_tx = 0;
int spwtx_fd = -1;
int time_tx_first = 1; /* Send Init bit of TimePacket only first time */
volatile int time_tick_tx_wrap = 0;

unsigned char cuctp_pkt_buf0[12] = {0};

/* Custom Interrupt Function */
void spwcuc_user_isr(unsigned int pimr, void *spwcuc)
{
	/* Handle IRQ here */

	if ( pimr & TICK_TX_WRAP_IRQ ) {

		/* Prepare CUC-TP Packet */
		spwcuc_create_cuctp_packet(spwcuc,254,254,0x1,time_tx_first,cuctp_pkt_buf0);

		/* Only INIT flag first TimePacket */
		if ( time_tx_first )
			time_tx_first = 0;

		/* Signal to task to send Time Packet */
		time_tick_tx_wrap = 1;
	}
}

void *cuc;

struct spwcuc_cfg time_tx_cfg = {
		.sel_out       = 0x1,
		.sel_in        = 0,  
		.mapping       = 18,
		.tolerance     = 8,
		.tid           = 1,
		.ctf           = 1,
		.cp            = 0, 
		.txen          = 1,
		.rxen          = 0,
		.pktsyncen     = 0,
		.pktiniten     = 0, 
		.pktrxen       = 0,
		.dla           = 254,
		.dla_mask      = 0xff,
		.pid           = 254,
		.offset        = 0
};

struct spwcuc_cfg time_rx_cfg = {
		.sel_out       = 0x0,
		.sel_in        = 0,  
		.mapping       = 18,
		.tolerance     = 8,
		.tid           = 1,
		.ctf           = 1,
		.cp            = 0, 
		.txen          = 0,
		.rxen          = 1,
		.pktsyncen     = 1,
		.pktiniten     = 1, 
		.pktrxen       = 1,
		.dla           = 254,
		.dla_mask      = 0xff,
		.pid           = 254,
		.offset        = 0x28
};

/* Set up a GRSPW device for Time-Code and Time Packets. If the
 * Link if not up
 *
 *  Enable TimeCode Reception/Transmit 
 */
int time_spw_setup(char *dev, int tx)
{
	int fd, status, i;
	unsigned int tc_ctrl;
	
	spwtx_fd = fd = open(dev, O_RDWR);
	if ( fd < 0 ) {
		return -1;
	}

	/* Setup Link Speed? */

	if ( tx == 1 ) {
		/* Enable TimeCode Tx:
		 *
		 * Time RX: Disable
		 * Time TX: Enable
		 * Time In: zero
		 */
		tc_ctrl = 0x40D;
	} else {
		/* Enable TimeCode Rx:
		 *
		 * Time RX: Enable
		 * Time TX: Disable
		 * Time In: zero
		 */
	
		tc_ctrl = 0x80D;
	}
	ioctl(fd, SPACEWIRE_IOCTRL_SET_TCODE_CTRL, tc_ctrl);

	/* Check that Link is up, max 100 times (~1 second) */
	i=100;
	do {
		ioctl(fd, SPACEWIRE_IOCTRL_GET_LINK_STATUS, &status);
		if ( status == 5 ) {
			/* Stop since already in run-mode */
			break;
		}

		rtems_task_wake_after(1);

		i--;
	} while ( i > 0 ) ;

	if ( status != 5 ) {
		printf("SpaceWire link %s not in run state: %d\n", dev, status);
		return -2;
	}

	/* If TX open driver for communication, needed in order to
	 * send TimePackets
	 */
	if ( tx ) {
		if (ioctl(fd, SPACEWIRE_IOCTRL_SET_TXBLOCK, 1) == -1) {
			printf("ioctl failed: SPACEWIRE_IOCTRL_SET_TXBLOCK\n");
			return -3;
		}

		if (ioctl(fd, SPACEWIRE_IOCTRL_START, 0) == -1) {
			return -4;
		}
	}

	return 0;
}

/* Setup Time Management */
int init_spwcuc(int tx, char *grspw_devname)
{
	int status;

	time_tx = tx;

	cuc = spwcuc_open(0);
	if ( cuc == NULL )
		return -1;

	/* Setup SpaceWire link for TimeCode/TimePacket handling */
	if ( (status=time_spw_setup(grspw_devname, tx)) ) {
		printf("Failed to init SpaceWire: %d\n", status);
		return -2;
	}

	/* Register Custom Interrupt handler */
	spwcuc_int_register(cuc, spwcuc_user_isr, cuc);

	/* Unmask interrutps at global IRQ controller */
	spwcuc_int_enable(cuc);

	/* Enable All interrupts */
	spwcuc_enable_irqs(cuc, 0x1fff);

	/* Configure SpaceWire-CUC */
	if ( tx ) {
		spwcuc_config(cuc, &time_tx_cfg);
	} else {
		spwcuc_config(cuc, &time_rx_cfg);
	}

	return 0;
}

/* Process stuff at a fixed time interval (10 times a second) */
void spwcuc_handling(void)
{
	if ( time_tick_tx_wrap ) {
		int i;

		/* This will only happen when in TX - TimeMaster */

		printf("PKT: ");
		for (i=0; i<12; i++)
			printf("%02x ", cuctp_pkt_buf0[i]);
		printf("\n");

		/* Write prepared packet */		
		write(spwtx_fd, &cuctp_pkt_buf0[0], 12);

		/* remember that we have already sent the latest 
		 * TimePacket
		 */
		time_tick_tx_wrap = 0;
	}
}

/********************** GRCTM ***********************/

#include <grctm.h>

/* custom stats counters (for debugging) */
volatile unsigned int nrirq = 0;	/* Number of IRQs */
volatile unsigned int p0 = 0, p1 = 0;

/* Device handle */
void *ctm;

void grctm_user_isr(unsigned int pimr, void *data)
{
	/* Handle IRQ HERE */

	nrirq++;
	if (pimr & PULSE0_IRQ)
		p0++;
	if (pimr & PULSE1_IRQ)
		p1++;
}

int init_ctm(int tx, int spw)
{

	ctm = grctm_open(0);
	if ( ctm == NULL ) {
		printf("Error Opening GRCTM[0]\n");
		return -1;
	}

	if (grctm_reset(ctm) < 0) {
		printf("Reset error.\n");
		return -2;
	}

	/* Clear statistics */
	grctm_clr_stats(ctm);

	/* Register Custom Interrupt handler */
	grctm_int_register(ctm, grctm_user_isr, ctm);

	/* Unmask interrutps at global IRQ controller */
	grctm_int_enable(ctm);

	printf("p0 = %p\nirq = %p\n", &p0, &nrirq);

	grctm_clear_irqs(ctm, -1);

	grctm_enable_irqs(ctm, PULSE0_IRQ|PULSE1_IRQ);

	if ( tx == 0 ) {
		if ( spw ) {
			/* Sync via SpaceWire TimeCodes & Time-Packets */
			grctm_enable_ext_sync(ctm);
		} else {
			/* Sync via TimeWire */
			grctm_enable_tw_sync(ctm);
		}
	}

	/*grctm_cfg_pulse(ctm, 0, 0x7, 0xf, 1, 0);*/
	grctm_cfg_pulse(ctm, 0, 0x7, 0xf, 1, 0);
	grctm_cfg_pulse(ctm, 1, 0x7, 0xf, 1, 0);

	grctm_enable_pulse(ctm, 0);
	grctm_enable_pulse(ctm, 1);

	return 0;
}

void ctm_handling(void)
{
	struct grctm_stats stats;

	/*** DO SOMETHING? CALLED EVERY TICK ***/

	/* Get IRQ Statistics from driver */
	grctm_get_stats(ctm, &stats);
#if 0
	if (stats.pulse > 50) {
		/* Do Something? */
		
	}
#endif
}
