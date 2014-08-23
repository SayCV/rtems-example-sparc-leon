/*
 * A RTEMS sample application using the GR1553BC MIL-1553B driver
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
#define CONFIGURE_MAXIMUM_TASKS             8
#define CONFIGURE_RTEMS_INIT_TASKS_TABLE
#define CONFIGURE_EXTRA_TASK_STACKS         (64 * RTEMS_MINIMUM_STACK_SIZE)
#define CONFIGURE_LIBIO_MAXIMUM_FILE_DESCRIPTORS 32
#define CONFIGURE_MAXIMUM_DRIVERS 16

#include <rtems/confdefs.h>

/* Include BM Log application configuration. We need to know if
 * the ethernet server is to be started, and if the log is to be
 * compressed.
 */
#include "config_bm.h"

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
#ifdef ETH_SERVER
#define CONFIGURE_DRIVER_AMBAPP_GAISLER_GRETH
#endif
#define CONFIGURE_DRIVER_PCI_GR_RASTA_IO        /* GR-RASTA-IO PCI Target Driver */

/* CONFIGURE GR-RASTA-IO Board */
#define CONFIGURE_DRIVER_AMBAPP_MCTRL           /* Driver for Memory controller needed when using SRAM on PCI board */
#define RASTA_IO_SRAM

/******** ADD GR1553B RT AND BM DRIVER **********/
#define CONFIGURE_DRIVER_AMBAPP_GAISLER_GR1553RT
#define CONFIGURE_DRIVER_AMBAPP_GAISLER_GR1553BM

#ifdef LEON2
  /* PCI support for AT697 */
  #define CONFIGURE_DRIVER_LEON2_AT697PCI
  /* AMBA PnP Support for GRLIB-LEON2 */
  #define CONFIGURE_DRIVER_LEON2_AMBAPP
#endif

#include <drvmgr/drvmgr_confdefs.h>

#include <stdio.h>
#include <stdlib.h>

/* If Ethernet server enable networking */
#ifdef ETH_SERVER
#define ENABLE_NETWORK
#else
#undef ENABLE_NETWORK
#endif

#undef ENABLE_NETWORK_SMC_LEON3

#include "config.c"

int init_rt_list(int *err);
int init_rt(void);
int rt_loop(void);

int init_bm(void);
int bm_log(void);

/* Ethernet Server functions */
extern int server_init(char *host, int port);
extern int server_wait_client(void);
extern int server_loop(void);
extern void server_stop();
volatile int client_avail = 0;

rtems_id taskEthid;
rtems_name taskEthname;

/* GR1553RT device on an AMBA-over-PCI */
/*#define AMBA_OVER_PCI*/

/* Enable/Disable RT Event Log printout */
#define EVLOG_PRINTOUT
/* Enable/disable printout of the RAW EventLog value */
/*#define EVLOG_PRINTOUT_RAW*/

#ifdef AMBA_OVER_PCI
  /* Translate Data Pointers from CPU-address into GR-RASTA-XXXX PCI address */
  #define TRANSLATE(adr) (uint16_t *)((unsigned int)(adr) | 0x1)
  /* Use 0x40000000-... at the PCI board as base for the descriptor table
   * Memory Layout in GR-RASTA-XXXX SRAM (over PCI):
   *   0x40000000 - 0x400003ff   (1kB)   Event Log
   *   0x40000400 - 0x400005ff   (512B)  Subaddress Table
   *   0x40000600 - 0x400045ff   (16kB)  Descriptors
   *   0x40005000 - ...          (1kB)   RT Data Buffers
   *   0x40010000 - 0x4001ffff   (64Kb)  BM Log DMA-Buffer
   */
  #define EV_TABLE_BASE TRANSLATE(0x40000000)
  #define SA_TABLE_BASE TRANSLATE(0x40000400)
  #define BD_TABLE_BASE TRANSLATE(0x40000600)
  /* Data Buffer area accessed by RT */
  #define RT_DATA_HW_ADR (struct rt_data_buffer *)0x40005000  /* The Address HW use */
  #define RT_DATA_CPU_ADR (struct rt_data_buffer *)0xA0005000 /* The address the CPU use to access the RT data. This should be autodetected! */

  /* Bus Monitor (BM) LOGGING BASE ADDRESS : In SRAM of GR-RASTA-XXXX */
  #define BM_LOG_BASE (0x40010000 | 1)
#else
  /* No translation of Data buffers needed */
  #define TRANSLATE(adr) (uint16_t *)(adr)
  /* Let driver dynamically allocate the descriptor table */
  #define EV_TABLE_BASE NULL
  #define SA_TABLE_BASE NULL
  #define BD_TABLE_BASE NULL
  #define RT_DATA_HW_ADR &RT_data
  #define RT_DATA_CPU_ADR RT_DATA_HW_ADR
  /* Dynamically allocate (BM) LOGGING BASE ADDRESS */
  #define BM_LOG_BASE NULL
#endif


rtems_task Init(
  rtems_task_argument ignored
)
{

	/* Initialize Driver manager and Networking, in config.c */
	system_init();

	/* Print device topology */	
	rtems_drvmgr_print_topo();

	if ( init_rt() ) {
		printf("Failed to initialize RT\n");
		exit(0);
	}

	if ( init_bm() ) {
		printf("Failed to initialize RT\n");
		exit(0);
	}

	if ( rt_loop() ) {
		printf("Failed to execute RT\n");
		exit(0);
	}

	exit( 0 );
}

#include <gr1553rt.h>
#include "pnp1553.h"

void rt_sa3_rx_isr(struct gr1553rt_list *list, unsigned int ctrl,
			int entry_next, void *data);
void rt_sa3_tx_isr(struct gr1553rt_list *list, unsigned int ctrl,
			int entry_next, void *data);
void *rt;

struct gr1553rt_cfg rtcfg =
{
	.rtaddress = 5,

	/* Mode code config: let all pass and be logged */
	.modecode = 0x2aaaaaaa,

	/* Highest time resolution */
	.time_res = 0,

	/* 512Byte SubAddress table, use malloc() */
	.satab_buffer = SA_TABLE_BASE,

	/* Event Log config */
	.evlog_buffer = EV_TABLE_BASE,
	.evlog_size = 1024,

	/* Tranfer descriptors config */
	.bd_count = 1024,
	.bd_buffer = BD_TABLE_BASE,
};


/* SUBADDRESS 1, BUS STATE BROADCASTED FROM BC:
 *  RX: WAIT/STARTUP/RUNNING/SHUTDOWN  (FROM BC)
 *  TX: DOWN/UP/DOWN              (RT INDICATE RUNNING STATUS TO BC)
 */
struct gr1553rt_list *sa1rx_list = NULL, *sa1tx_list = NULL;
struct gr1553rt_list_cfg sa1_cfg = 
{
	.bd_cnt = 1,
};

/* SUBADDRESS 2, Plug&Play information:
 *  TX: VENDOR|DEVICE|VERSION|CLASS|STRING
 *  RX: NOTHING
 */
struct gr1553rt_list *sa2tx_list = NULL;
struct gr1553rt_list_cfg sa2_cfg = 
{
	.bd_cnt = 1,
};


/* SUBADDRESS 3, BC<->RT Data transfers. ~1kB/sec:
 *
 *  RX: BC Transfer Data in 64byte block.
 *  TX: Received BC data is copied here.
 */
struct gr1553rt_list *sa3tx_list = NULL;
struct gr1553rt_list *sa3rx_list = NULL;

struct gr1553rt_list_cfg sa3_cfg = 
{
	.bd_cnt = 16, /* two per major frame */
};


int rxbuf_curr = 0;

/* All RT Data buffers */
struct rt_data_buffer {
	/* Startup/Shutdown parameters the BC control */
	unsigned short bus_status; /* Wait */
	unsigned short rt_status; /* Down */

	/* Information about RT device that BC read */
	struct pnp1553info pnp1553_info;

	/* Transfer buffers */
	uint16_t rxbufs[16][32];
	uint16_t txbufs[16][32];
};

/* Initial State of RT Data */
struct rt_data_buffer RT_data_startup = 
{
	.bus_status = 0,
	.rt_status = 0,
	.pnp1553_info = 
	{
		.vendor = 0x0001,
		.device = 0x0001,
		.version = 0,
		.class = 0,
		.subadr_rx_avail = 0x0003,
		.subadr_tx_avail = 0x0007,
		.desc = "GAISLER RTEMS DEMO1",
	},
};

#ifndef AMBA_OVER_PCI
	/* Data Buffers the RT Hardware Access */
	struct rt_data_buffer RT_data;
#endif
/* Pointer to RT_Data structure. For AMBA-over-PCI the pRT_data pointer
 * should be automatically be calculated.
 */
struct rt_data_buffer *pRT_data = RT_DATA_CPU_ADR;
struct rt_data_buffer *pRT_data_hw = RT_DATA_HW_ADR;

int init_rt_list(int *err)
{
	int i;
	int irq, next;

	/*** SUBADDRESS 1 - BUS STATUS ***/

	/* Make driver allocate list description */
	sa1rx_list = NULL;
	if ( (*err = gr1553rt_list_init(rt, &sa1rx_list, &sa1_cfg)) ) {
		return -1;
	}
	sa1tx_list = NULL;
	if ( (*err = gr1553rt_list_init(rt, &sa1tx_list, &sa1_cfg)) ) {
		return -2;
	}
	/* Setup descriptors to receive STATUS WORD and transmit current
	 * RT status.
	 *
	 * RX and TX: circular ring with one descriptor.
	 */
	if ( (*err = gr1553rt_bd_init(sa1rx_list, 0, 0, &pRT_data_hw->bus_status, 0)) )
		return -3;
	if ( (*err = gr1553rt_bd_init(sa1tx_list, 0, 0, &pRT_data_hw->rt_status, 0)) )
		return -4;


	/*** SUBADDRESS 2 - PnP READ ***/

	sa2tx_list = NULL;
	if ( (*err = gr1553rt_list_init(rt, &sa2tx_list, &sa2_cfg)) ) {
		return -10;
	}
	if ( (*err = gr1553rt_bd_init(sa2tx_list, 0, 0, 
	                         (uint16_t *)&pRT_data_hw->pnp1553_info, 0)) )
		return -11;


	/*** SUBADDRESS 3 - BC<->RT DATA Transfer ***/

	sa3tx_list = NULL;
	sa3rx_list = NULL;
	if ( (*err = gr1553rt_list_init(rt, &sa3tx_list, &sa3_cfg)) ) {
		return -20;
	}
	if ( (*err = gr1553rt_list_init(rt, &sa3rx_list, &sa3_cfg)) ) {
		return -21;
	}
	/* Install IRQ handlers for RX/TX (only RX is used) */
	if ( gr1553rt_irq_sa(rt, 3, 1, rt_sa3_tx_isr, rt) ) {
		return -22;
	}
	if ( gr1553rt_irq_sa(rt, 3, 0, rt_sa3_rx_isr, rt) ) {
		return -22;
	}
	/* Transmit And Receive list */
	for ( i=0; i<16; i++) {
		/* Next Descriptor, with wrap-around */
		next = i + 1;
		if ( next == 16 )
			next = 0;

		/* Enable RX IRQ when last transfer within an Major
		 * frame is received. We have up 25ms to prepare
		 * the data the BC will request.
		 */
		irq = 0;
		if ( i & 1 )
			irq = GR1553RT_BD_FLAGS_IRQEN;

		if ( (*err = gr1553rt_bd_init(sa3tx_list, i, 0,
                             &pRT_data_hw->txbufs[i][0], next)) ) {
			return -30;	      
		}

		if ( (*err = gr1553rt_bd_init(sa3rx_list, i, irq,
                              &pRT_data_hw->rxbufs[i][0], next)) ) {
			return -31;	      
		}
	}

	return 0;
}

struct rt_sas_config {
	unsigned int mask;
	unsigned int opts;
	struct gr1553rt_list **rxlist;
	struct gr1553rt_list **txlist;
} rtsa_cfg[32] =
{
/* SEE HW MANUAL FOR BIT DEFINITIONS 
 *
 * Mode code:      all give IRQ and is logged.
 *
 * Non-mode codes: are not logged or IRQed by default, only
 *                 when explicitly defined by descriptor config.
 */
	/* 00 */ {0xffffffff, 0x00000},	/* Mode code - ignored */
	/* 01 */ {0xffffffff, 0x38181, &sa1rx_list, &sa1tx_list}, /* Startup/shutdown sub address */
	/* 02 */ {0xffffffff, 0x39090, NULL, &sa2tx_list}, /* PnP: VENDOR|DEVICE|VERSION|CLASS... Limit to 32 byte */
	/* 03 */ {0xffffffff, 0x38080, &sa3rx_list, &sa3tx_list}, /* 64byte Data transfers */
	/* 04 */ {0xffffffff, 0x00000},
	/* 05 */ {0xffffffff, 0x00000},
	/* 06 */ {0xffffffff, 0x00000},
	/* 07 */ {0xffffffff, 0x00000},
	/* 08 */ {0xffffffff, 0x00000},
	/* 09 */ {0xffffffff, 0x00000},
	/* 10 */ {0xffffffff, 0x00000},
	/* 11 */ {0xffffffff, 0x00000},
	/* 12 */ {0xffffffff, 0x00000},
	/* 13 */ {0xffffffff, 0x00000},
	/* 14 */ {0xffffffff, 0x00000},
	/* 15 */ {0xffffffff, 0x00000},
	/* 16 */ {0xffffffff, 0x00000},
	/* 17 */ {0xffffffff, 0x00000},
	/* 18 */ {0xffffffff, 0x00000},
	/* 19 */ {0xffffffff, 0x00000},
	/* 20 */ {0xffffffff, 0x00000},
	/* 21 */ {0xffffffff, 0x00000},
	/* 22 */ {0xffffffff, 0x00000},
	/* 23 */ {0xffffffff, 0x00000},
	/* 24 */ {0xffffffff, 0x00000},
	/* 25 */ {0xffffffff, 0x00000},
	/* 26 */ {0xffffffff, 0x00000},
	/* 27 */ {0xffffffff, 0x00000},
	/* 28 */ {0xffffffff, 0x00000},
	/* 29 */ {0xffffffff, 0x00000},
	/* 30 */ {0xffffffff, 0x00000},
	/* 31 */ {0xffffffff, 0x1e0e0},	/* Mode code - ignored */
};

void init_rt_sa(void)
{
	int i;

	for (i=0; i<32; i++) {
		gr1553rt_sa_setopts(rt, i, rtsa_cfg[i].mask, rtsa_cfg[i].opts);
		if ( rtsa_cfg[i].rxlist )
			gr1553rt_sa_schedule(rt, i, 0, *rtsa_cfg[i].rxlist);
		if ( rtsa_cfg[i].txlist )
			gr1553rt_sa_schedule(rt, i, 1, *rtsa_cfg[i].txlist);
	}
}

int rt_irq_cnt=0;

unsigned int rt_isr(struct gr1553rt_list *list, int entry, void *data)
{
	rt_irq_cnt++;

	if ( list == sa3rx_list ) {
	
	} else if ( list == sa3tx_list ) {
	
	}

	/* Default action is to clear the DATA-VALID flag and
	 * TIME, BC, SZ, RES
	 */
	return 0x83ffffff;
}

/* ERROR IRQ (DMA Error or RT Table access error) */
void rt_err_isr(int err, void *data)
{
	printk("ERROR IRQ: 0x%x\n", err);
}

/* Mode Code Received */
void rt_mc_isr(int mcode, unsigned int entry, void *data)
{
	printk("MC%d IRQ: 0x%08x\n", mcode, entry);
}

int init_rt(void)
{
	int status, err;

	/* Initialize default values of data buffers */
	memcpy(pRT_data, &RT_data_startup, sizeof(struct rt_data_buffer));

	rxbuf_curr = 0;

	/* Print List:
	 *   gr1553bc_show_list(list, 0);
	 */

	/* Aquire RT device */
	rt = gr1553rt_open(0);
	if ( !rt ) {
		printf("Failed to open RT[%d]\n", 0);
		return -1;
	}

	/* Configure driver before setting up lists */
	if ( gr1553rt_config(rt, &rtcfg) ) {
		printf("Failed to configure RT driver\n");
		return -1;
	}

	/* Assign Error IRQ handler */
	if ( gr1553rt_irq_err(rt, rt_err_isr, rt) ) {
		printf("Failed to register ERROR IRQ function\n");
		return -1;
	}

	/* Assign ModeCode IRQ handler */
	if ( gr1553rt_irq_mc(rt, rt_mc_isr, rt) ) {
		printf("Failed to register ERROR IRQ function\n");
		return -1;
	}

	/* Set up lists and schedule them on respective RT subaddress.
	 * Also, register custom IRQ handlers on some transfer
	 * descriptors.
	 */
	if ( (status=init_rt_list(&err)) != 0 ) {
		printf("Failed to init lists: %d : %d\n", status, err);
		return -1;
	}

	/* Set up configuration options per RT sub-address */
	init_rt_sa();

	/* Start communication */
	status = gr1553rt_start(rt);
#if 0
	/* Register standard IRQ handler when an error occur */
	if ( gr1553bc_irq_setup(bc, bc_isr, bc) ) {
		printf("Failed to register standard IRQ handler\n");
		return -2;
	}

	/* Start previously created BC list */
	status = gr1553bc_start(bc, list, NULL);
	if ( status ) {
		printf("Failed to start BC: %d\n", status);
		return -3;
	}

	printf("            MAJOR MINOR SLOT\n");
	while ( 1 ) {
		if ( gr1553bc_indication(bc, &mid) ) {
			printf("Error getting current MID\n");
			gr1553bc_show_list(list, 0);
			return -1;
		}
		
		printf("INDICATION: %02x %02x %02x\n",
			GR1553BC_MAJID_FROM_ID(mid),
			GR1553BC_MINID_FROM_ID(mid),
			GR1553BC_SLOTID_FROM_ID(mid));
		
		if ( bm_log() ) {
			printf("BM Log failed\n");
			return -2;
		}
		rtems_task_wake_after(10);
		/*sleep(1);*/
	}
#endif
	return 0;
}

int state = 0;
int init_state = 0;

int rt_startup(void)
{
	printf("RT Starting up\n");

	return 0;	
}

int rt_run()
{
	printf("Bus in running mode\n");

	return 0;
}

int rt_init_state(void)
{
	switch ( init_state ) {
	default:
	case 0:	/* Wait for Startup Message */

		/* Check if BC wants us to startup */
		if ( (0x00ff & pRT_data->bus_status) > 0 ) {

			/* Try to startup RT */
			if ( rt_startup() ) {
				printf("RT Startup failed\n");
				return -1;
			}

			/* Step to next state */
			init_state = 1;

			/* Signal to BC we have started up */
			pRT_data->rt_status = 1;
		}
		break;

	case 1:
		/* Check if BC say all RTs are started up and ready to go */

		if ( (0x00ff & pRT_data->bus_status) > 1 ) {
			if ( rt_run() ) {
				printf("RT Run mode failed\n");
				return -1;
			}
			init_state = 1;
			return 1;
		}
		break;
	}

	return 0;
}

/* Check new Data has arrived from BC. In that case data is copied
 * to RT transmit.
 */
void bc_rt_data_transfer(void)
{
#if 0
	int curr;

	/* Do nothing, our ISR handles data copying */

	/* Get Received data by looking at the current descriptor address 
	 *
	 */
	gr1553rt_indication(rt, 3, NULL, &curr);

	/* Have we received more data? */
	if ( curr == rxbuf_curr ) {
		return;
	}

	while ( curr != rxbuf_curr ) {

		/* Handle buffer 'rxbuf_curr' */
		memcpy(pRT_data->txbufs[rxbuf_curr], pRT_data->rxbufs[rxbuf_curr], 64);

		/* Next buffer will be */
		rxbuf_curr++;
		if ( rxbuf_curr >= 16 ) {
			rxbuf_curr = 0;
		}
	}
#endif
}

#ifdef EVLOG_PRINTOUT
void rt_process_evlog(void)
{
	unsigned int events[64], ev;
	int i, cnt, type, samc, bc, size, result, irq;
	char *type_str, *bc_str, *result_str, *irq_str;

	do {
		/* Get up to 64 events from Event log
		 *
		 * This method can be used to handle logged transmissions
		 * for non-interrupted or together with interrupted
		 * transmissions.
		 */
		cnt = gr1553rt_evlog_read(rt, events, 64);
		if ( cnt < 1 )
			break;

		/* Process the entries */
		for ( i=0; i<cnt; i++) {
			ev = events[i];

			/* Decode */
			irq = ev >> 31;
			type = (ev >> 29) & 0x3;
			samc = (ev >> 24) & 0x1f;
			bc =  (ev >> 9) & 0x1;
			size = (ev >> 3) & 0x3f;
			result =  ev & 0x7;

			bc_str = "";
			irq_str = "";
			result_str = "";

			if ( bc )
				bc_str = "BC ";
			if ( result != 0 )
				result_str = " ERROR";
			if ( irq )
				irq_str = "IRQ ";

			switch ( type ) {
				case 0:
					type_str = "TX";
					break;

				case 1:
					type_str = "RX";
					break;

				case 2:
					type_str = "MC";
					break;

				default:
					type_str = "UNKNOWN";
					break;
			}
#ifdef EVLOG_PRINTOUT_RAW
			printf("EV: %s%02x: %s%slen=%d%s (%08x)\n",
				type_str, samc, irq_str, bc_str, size,
				result_str, ev);
#else
			printf("EV: %s%02x: %s%slen=%d%s\n",
				type_str, samc, irq_str, bc_str, size,
				result_str);
#endif
		}

	} while ( cnt == 64 );
}
#endif

int rt_debug_majfrm = 0;
int rt_debug_blockno = 0;
int rt_sa3_irqs = 0;

void rt_sa3_rx_isr
	(
	struct gr1553rt_list *list, 
	unsigned int ctrl,
	int entry_next,
	void *data
	)
{
	static int last_entry = 0;

	/* We have received two blocks of data: 128-bytes total, located
	 * in our receive buffers: rxbufs[MAJOR_FRAME][block].
	 *
	 * Copy received data to transmit buffers
	 */
	int majfrm = last_entry / 2;
	int blockno = last_entry & 1;
	unsigned int status;

	/* Re enable RX IRQ */
	status = GR1553RT_BD_FLAGS_IRQEN;
	gr1553rt_bd_update(sa3rx_list, last_entry+1, &status, NULL);

	memcpy(&pRT_data->txbufs[last_entry+0][0], &pRT_data->rxbufs[last_entry+0][0], 64);
	memcpy(&pRT_data->txbufs[last_entry+1][0], &pRT_data->rxbufs[last_entry+1][0], 64);

	last_entry += 2;
	if ( last_entry >= 16 )
		last_entry = 0;

	rt_debug_majfrm = majfrm;
	rt_debug_blockno = blockno;

	rt_sa3_irqs++;
}

/* Descriptors or SA-table do not have Interrupts enabled, so we will not
 * end up here.
 */
void rt_sa3_tx_isr
	(
	struct gr1553rt_list *list, 
	unsigned int ctrl,
	int entry_next,
	void *data
	)
{
}

int rt_loop(void)
{
	int status;

	state = 0;
	init_state = 0;

	while ( 1 ) {
		/* Answer BC's requests */
		switch ( state ) {
		default:
		case 0: /* Initial State */
			status = rt_init_state();
			if ( status < 0 ) {
				return -1;
			} else if ( status > 0 ) { /* Done with INIT - Switch? */
				printf("RT into state 1\n");
				state = 1;
			}
			break;

		case 1: /* Communication State */
			bc_rt_data_transfer();
#ifdef EVLOG_PRINTOUT
			rt_process_evlog();
#endif
			break;
		}

		if ( bm_log() ) {
			printf("BM Log failed\n");
			return -2;
		}

		/* Sleep 1 ticks */
		rtems_task_wake_after(1);
	}
	return 0;
}

#include "bm_logger.c"
