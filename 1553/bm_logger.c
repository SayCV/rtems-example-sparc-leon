#include <gr1553bm.h>

void *bm = NULL;
int bm_log_entry_cnt = 0;

#ifdef COMPRESSED_LOGGING
/* Compressed log */
struct bm_cmp_log {
	unsigned int *base;
	unsigned int *head;
	unsigned int *tail;
	unsigned int *end;
	uint64_t time;
	uint64_t lastlogtime;
};
struct bm_cmp_log cmp_log;
#define BM_CMP_LOG_SIZE 0x200000    /* 2Mb */
#define BM_CMP_LOG_CNT  (BM_CMP_LOG_SIZE/4)

int bm_log_copy(
	unsigned int dst,
	struct gr1553bm_entry *src,
	int nentries,
	void *data
	);

/* Add a number of words to "compressed" Log */
void log_cmp_add(struct bm_cmp_log *log, unsigned int *words, int cnt)
{
	int i;

	for (i=0; i<cnt; i++) {
		*log->head = words[i];
		log->head++;
		if ( log->head >= log->end )
			log->head = log->base;
		if ( log->head == log->tail ) {
			log->tail++;
			if ( log->tail >= log->end )
				log->tail = log->base;
		}
	}
}

int log_cmp_take(struct bm_cmp_log *log, unsigned int *words, int max)
{
	int i;

	for (i=0; i<max; i++) {
		if ( log->tail == log->head ) {
			/* log empty */
			break;
		}
		words[i] = *log->tail;
		log->tail++;
		if ( log->tail >= log->end )
			log->tail = log->base;
	}

	return i;
}

/* Add an Control entry to log.
 *
 * CTRL is 29-bits
 */
void log_cmp_add_ctrl(struct bm_cmp_log *log, int ctrl)
{
	unsigned int word = ctrl | 0xe0000000;
	log_cmp_add(log, &word, 1);
}

int dummy(void)
{
	static int i=0;
	return i++;
}

/* Copy Data To a larger software buffer
 *
 * dst       Argument not used
 * src       Source DMA buffer start
 * nentires  Must process the number of entries.
 */
int bm_log_copy(
	unsigned int dst,
	struct gr1553bm_entry *src,
	int nentries,
	void *data
	)
{
	unsigned int words[3];
	int wc;
	struct bm_cmp_log *log = data;
	uint64_t currtime, logtime64, ll_time64, time64;
	unsigned int time24, logtime24;
	int cnt=0;

	/* Sample Current Time */
	gr1553bm_time(bm, &currtime);
	time64 = currtime & ~0x00ffffff;
	time24 = currtime &  0x00ffffff;

	/* Get LastLog Time, but ignore lowest 13 bits */
	ll_time64 = log->time & ~0x1fff;

	/* We know that the current time must be 
	 * more recent than the time in the logs
	 * since the log length has been prepared
	 * before calling gr1553bm_time() here.
	 *
	 * We also know that this function is
	 * called so often that 24-bit time may
	 * only have wrapped once against current
	 * time sample.
	 */

	while ( nentries-- ) {
		/* Compress One entry */

		/*** Calculate Exact time of one LOG ENTRY ***/

		logtime24 = src->time & 0x00ffffff;
		/* Calculate Most significant bits in time */
		if ( logtime24 < time24 ) {
			/* No Wrap occurred */
			logtime64 = time64;
		} else {
			/* Wrap occurred.  */
			logtime64 = time64 - 0x1000000;
		}
		/* Set lowest 24-bits */
		logtime64 |= logtime24;

/* FOR DEBUGGING WHEN BUFFER HAS BEEN FULL. 
 *
 * WE SHOULD WRITE ERROR CONTROL WORD HERE.
 */
		if ( log->lastlogtime > logtime64 ) {
			/* Stop BM logging */
			*(volatile unsigned int *)0x800005c4 = 0;
			printf("LOG LAST TIME WAS PRIOR:\n");
			printf("  Last: 0x%llx\n", log->lastlogtime);
			printf("  Now:  0x%llx:\n", logtime64);
			printf("  time64:  0x%llx:\n", time64);
			printf("  time24:  0x%x:\n", time24);
			printf("  currtime:  0x%llx:\n", currtime);
			printf("  ll_time64:  0x%llx:\n", ll_time64);
			printf("  cnt: %d\n", cnt);
			printf("  words[i-4]: 0x%08x\n", (unsigned int)(src-2)->time);
			printf("  words[i-3]: 0x%08x\n", (unsigned int)(src-2)->data);
			printf("  words[i-2]: 0x%08x\n", (unsigned int)(src-1)->time);
			printf("  words[i-1]: 0x%08x\n", (unsigned int)(src-1)->data);
			printf("  words[i+0]: 0x%08x\n", (unsigned int)src->time);
			printf("  words[i+1]: 0x%08x\n", (unsigned int)src->data);
			dummy();
			asm volatile("ta 0x1\n\t");
		}
		log->lastlogtime = logtime64;

		/* Do we need to write down time in a longer format? We
		 * Compare with the time of the last recoded entry.
		 *
		 * The extra 30 bits of time is written
		 * if the 13-bit time is overflown since last
		 * sample.
		 */
		if ( (logtime64 & ~0x1fff) > ll_time64 ) {
			/* Write down long time before data entry 
			 * The 2 MSB bits must be 0b10
			 */
			ll_time64 = logtime64 & ~0x1fff;

			/* Get 30-bit MSB Time from 64-bit time */
			words[0] = 0x80000000 | ((logtime64>>13) & 0x3fffffff);
			wc = 1;
		} else {
			wc = 0;
		}

		if ( (src->data & (0x3<<17)) != 0 ) {
			/* Error of some kind. Log error 
			 * The 3 MSB bits must be 0b110
			 */
			words[wc] = 0xc0000000 | ((src->data >> 17) & 0x3);
			wc++;
		}
		/* Log Transfer 
		 * The MSB bit must be 0
		 */
		words[wc] =
			((logtime64 & 0x1fff)<<18) |	/* 13-bit time */
			((src->data & 0x80000)>>2) |	/* Bus bit */
			(src->data & 0x1ffff);		/* DATA and WTP */
		wc++;

		/* Write Words to buffer */
		log_cmp_add(log, &words[0], wc);

		src++;
		cnt++;
	}

	/* Save complete last log time */
	log->time = logtime64;

	return 0;
}

#endif

#ifdef ETH_SERVER
int eth_setup(void);
#endif

struct gr1553bm_config bmcfg =
{
	.time_resolution = 0,	/* Highest time resoulution */
	.time_ovf_irq = 1,	/* Let IRQ handler update time */
	.filt_error_options = 0xe, /* Log all errors */
	.filt_rtadr = 0xffffffff,/* Log all RTs and Broadcast */
	.filt_subadr= 0xffffffff,/* Log all sub addresses */
	.filt_mc = 0x7ffff,	/* Log all Mode codes */
	.buffer_size = 16*1024,/* 16K buffer */
	.buffer_custom = (void *)BM_LOG_BASE,	/* Let driver allocate dynamically or custom adr */
#ifdef COMPRESSED_LOGGING
	.copy_func = bm_log_copy, /* Custom Copying to compressed buffer */
	.copy_func_arg = &cmp_log,
#else
	.copy_func = NULL,	/* Standard Copying */
	.copy_func_arg = NULL,
#endif
	.dma_error_isr = NULL,	/* No custom DMA Error IRQ handling */
	.dma_error_arg = NULL,
};

/* Set up BM to log eveything */

int init_bm(void)
{
	int status;

	/* Aquire BM device */
	bm = gr1553bm_open(0);
	if ( !bm ) {
		printf("Failed to open BM[%d]\n", 0);
		return -1;
	}

#ifdef COMPRESSED_LOGGING
	cmp_log.base = (unsigned int *)malloc(BM_CMP_LOG_SIZE);
	if ( cmp_log.base == NULL ) {
		/* Failed to allocate log buffer */
		return -2;
	}
	cmp_log.head = cmp_log.base;
	cmp_log.tail = cmp_log.base;
	cmp_log.end = cmp_log.base + BM_CMP_LOG_CNT;
	cmp_log.time = 0;
	cmp_log.lastlogtime = 0;

	/* Add initialial entry in log (START) */
	log_cmp_add_ctrl(&cmp_log, 0);
#endif

	/* Register standard IRQ handler when an error occur */
	if ( gr1553bm_config(bm, &bmcfg) ) {
		printf("Failed to configure BM driver\n");
		return -3;
	}

#ifdef ETH_SERVER
	/* Set up and start Ethernet server */
	if ( eth_setup() ) {
		printf("Failed setting up Ethernet\n");
		return -4;
	}
#endif

#ifdef BM_WAIT_CLIENT
	/* Wait for client to conect before proceeding */
	printf("Waiting for TCP/IP client to connect\n");
	while ( client_avail == 0 ) {
		/* Wait 10 ticks */
		rtems_task_wake_after(10);
	}
#endif

	/* Start BM Logging as configured */
	status = gr1553bm_start(bm);
	if ( status ) {
		printf("Failed to start BM: %d\n", status);
		return -4;
	}

#ifdef COMPRESSED_LOGGING
	{
		uint64_t time1553;
		unsigned int words[2];

		gr1553bm_time(bm, &time1553);

		/* Add initialial time entry in log */
		words[0] = 0x80000000 | ((time1553>>13) & 0x3fffffff);
		words[1] = (time1553 & 0x1fff) << 18; /* Empty Data */
		log_cmp_add(&cmp_log, &words[0], 2);
	}
#endif

	return 0;
}

/* Temporary buffer */
struct gr1553bm_entry bm_log_entries[256];
int nentries_log[5000] = {0,0};

/* Handle BM LOG (empty it) */
int bm_log(void)
{
	int nentries, max, tot;

	nentries = 10000000; /* check that is overwritten */
	if ( gr1553bm_available(bm, &nentries) ) {
		printf("Failed to get number of available BM log entries\n");
		return -2;
	}
	nentries_log[nentries]++;

	tot = 0;
	do {
		max = 128;
		if ( gr1553bm_read(bm, &bm_log_entries[0], &max) ) {
			printf("Failed to read BM log entries\n");
			return -3;
		}
		tot += max;

#ifndef COMPRESSED_LOGGING
		/* Handle Copied BM Log here. 
		 */
#else
		 /* When doing custom copy, the copy function has 
		  * already handled the data. Nothing to do.
		  */
#endif
		/* Read as long as the BM driver fills our buffer */
	} while ( max == 128 );

	if ( tot < nentries ) {
		printf("BM Failed to read all entries: %d, %d\n",
			nentries, max);
		return -4;
	}

	/* Update stats */
	bm_log_entry_cnt += tot;
	/*printf("BM Entries: %d (time: %llu)\n", bm_log_entry_cnt, time1553);*/
	/*printf("BM Entries: %d\n", bm_log_entry_cnt);*/

	return 0;
}

#ifdef ETH_SERVER

/* Ethernet TCP/IP Server Task */
void task_eth_server(rtems_task_argument argument)
{
	int err;

	printf("ETH Server task started\n");

	while ( 1 ) {

		/* Wait for a new client to conenct to ETH server */
		printf("ETH: Waiting for remote client to connect\n");

		err = server_wait_client();
		if ( err ) {
			printf("ETH: Ethernet server wait client failure: %d\n", err);
			break;
		}

		client_avail = 1;

		/* A new client connected, ready to be served */
		printf("ETH: Client connected to ETH Server\n");

		err = server_loop();
		client_avail = 0;
		if ( err ) {
			printf("ETH: Ethernet server loop failure: %d\n", err);
			break;
		}

		/* A client has disconnected, wait for a new connection */
		printf("ETH: Client disconnected from ETH Server\n");
	}

	/* Stop any open socket befor quitting */
	server_stop();

	/* Failure */
	exit(-1);
}

/* Setup Ethernet TCP/IP server */
int eth_setup(void)
{
	rtems_status_code status;
	int err;

	/* Create Ethernet Server task */
	taskEthname = rtems_build_name( 'S', 'E', 'R', 'V' );

	status = rtems_task_create (taskEthname,
			1,
			64*1024,
			0,
			RTEMS_LOCAL | RTEMS_FLOATING_POINT,
			&taskEthid);
	if (status != RTEMS_SUCCESSFUL) {
		printf ("Can't create task: %d\n", status);
		return -1;
	}

	err = server_init("192.168.0.67", ETHSRV_PORT);
	if ( err ) {
		printf("Error initializing Ethernet Server: %d\n", err);
		return -1;
	}

	/* Start Ethernet server thread */
	status = rtems_task_start(taskEthid, task_eth_server, 0);
	if ( status != RTEMS_SUCCESSFUL ) {
		printf("Failed to start task A\n");
		exit(-1);
	}

	return 0;
}

#include "ethsrv.c"

#endif
