#include <stdio.h>
#include <stdlib.h>

#include <gr1553bc.h>

void bc_rt_data_isr(int comfrm);
void bc_rt_data_init(void);

/* The BC list */
struct gr1553bc_list *list;
struct gr1553bc_major *initial_major;
struct gr1553bc_major *timesync_major;
struct gr1553bc_major *comframes[8];
struct gr1553bc_major *final_major;

/* Initial Major Frame */
struct {
	int minor_cnt;
	struct gr1553bc_minor_cfg minor_cfgs[3];
} initial_cfg =
{
	.minor_cnt = 3,
	.minor_cfgs = 
		{
			/* 150ms repetitive */
/* Start up Messages */	{.slot_cnt = 32, .timeslot = 50000},
/* RT Status Requests */{.slot_cnt = 20, .timeslot = 50000},
/* RT PnP info request*/{.slot_cnt = 10, .timeslot = 50000},
		},
};

/* On-board Time Sync Major Frame */
struct gr1553bc_major_cfg timesync_cfg =
{
	.minor_cnt = 1,
	.minor_cfgs =
		{
			/* Execute once before entering communication frame
			 * 3 slots for:
			 *  1. Wait Timer sync Trigger
			 *  2. SEND NEXT TIME MESSAGE
			 *  3. UNUSED
			 */
			{.slot_cnt = 3, .timeslot = 60000},
		},
};

/* Define Structure that hold configuration for frame 2..9.
 * NOTE: must have same layout as struct gr1553bc_major_cfg
 *
 * 9 Minor Frames per Major frame, up to 32 Message slots per Minor
 * Each major frame execute 125ms.
 */
struct {
	int minor_cnt;
	struct gr1553bc_minor_cfg minor_cfgs[9];
} frame_1_to_8_cfg = 
{
	.minor_cnt = 9,
	.minor_cfgs =
		{
			{.slot_cnt = 4, .timeslot = 100}, /* Time/ComFrame Sync */
			{.slot_cnt = 4, .timeslot = 6225},
			{.slot_cnt = 4, .timeslot = 6225},
			{.slot_cnt = 4, .timeslot = 6225},
			{.slot_cnt = 4, .timeslot = 6225},
			{.slot_cnt = 32, .timeslot = 12500},
			{.slot_cnt = 32, .timeslot = 25000},
			{.slot_cnt = 32, .timeslot = 12500},
			{.slot_cnt = 32, .timeslot = 50000},
		},
};

/* Initial Major Frame */
struct gr1553bc_major_cfg final_cfg =
{
	.minor_cnt = 1,
	.minor_cfgs = 
		{
			/* 50ms repetitive */
			{.slot_cnt = 16, .timeslot = 50000},
		},
};

/*** Predefined messages ***/

uint16_t bc_shutdown_messageA[1] __attribute__ ((aligned (4))) =
{
	0xA001,
};

uint16_t bc_shutdown_messageB[1] __attribute__ ((aligned (4))) =
{
	0xB001,
};

uint16_t bc_startup_messageA[1] __attribute__ ((aligned (4))) = 
{
	0xA001,
};

uint16_t bc_startup_messageB[1] __attribute__ ((aligned (4))) = 
{
	0xB001,
};

uint16_t bc_running_messageA[1] __attribute__ ((aligned (4))) = 
{
	0xA002,
};

uint16_t bc_running_messageB[1] __attribute__ ((aligned (4)))= 
{
	0xB002,
};

/* Status of RTs on BusA and BusB. */
uint16_t rt_statusA[32] __attribute__ ((aligned (4)));
uint16_t rt_statusB[32] __attribute__ ((aligned (4)));
/* "PnP" info read from RT during initialization and after a RT has
 * answered from basic questions.
 */
struct pnp1553info rt_pnp_info[32];

/*** SYNC VARIABLES ***/
/* Data sent to RTs before time sync message */
uint16_t bc_timesync_nexttime __attribute__ ((aligned (4))) = 0;
/* Data sent to RTs in communication Frame Sync message. identifies 
 * current communication frame.
 */
uint16_t bc_frmsync[8] __attribute__ ((aligned (4))) = {0,1,2,3,4,5,6,7};

/* Write to non-zero to shut down*/
int bc_shutdown = 0;

int test_max_slot_free(void);
int test_max_slot_alloc(void);

void time_sync_begin_com_frame0(union gr1553bc_bd  *bd, void *data)
{
	/* In IRQ context, 1553 has begun executing communication frame 0 
	 * again.
	 */
#if 0
	int mid = (int)data;

	/* IRQ GENERATED FROM MID 'mid' */
#endif
}

/* In IRQ context:
 *
 * Called when each Communication Frame starts.
 */
void bc_com_frame_sync(union gr1553bc_bd  *bd, void *data)
{
	int mid = (int)data;
	int major_no = GR1553BC_MAJID_FROM_ID(mid);
	int comframe = major_no - 2;
	struct gr1553bc_status status;

	gr1553bc_status(bc, &status);
	/* Print Comframe number and current time */
	printk("CF%d:%x\n", comframe, status.time);

	/* Process RT transfer data */
	bc_rt_data_isr(comframe);
}

int init_frame_exec_count = 0;
int init_frame_exec_err_count = 0;

void list_entry_isr(union gr1553bc_bd  *bd, void *data)
{
	int mid = (int)data;

	if ( GR1553BC_ID(0, 0, 31) ==  mid ) {
		init_frame_exec_count++;
	} else {
		init_frame_exec_err_count++;
	}

}

int init_bc_list(void)
{
	int i;
	int mid;

/***** CREATE MAJOR FRAME STRUCTURES *****/
	memset(rt_statusA, 0, sizeof(rt_statusA));
	memset(rt_statusB, 0, sizeof(rt_statusB));
	memset(rt_pnp_info, 0, sizeof(rt_pnp_info));

	bc_rt_data_init();

	/* Create Initial Major frame */
	if ( gr1553bc_major_alloc_skel(&initial_major, 
	                        (struct gr1553bc_major_cfg *)&initial_cfg) ) {
		exit(-2);
	}

	/* Create Time Sync Major frame */
	if ( gr1553bc_major_alloc_skel(&timesync_major, &timesync_cfg) ) {
		exit(-2);
	}

	/* Create 8 similar Major frames with minor frames and message slots */
	for (i=0; i<8; i++) {
		if ( gr1553bc_major_alloc_skel(&comframes[i],
			(struct gr1553bc_major_cfg *)&frame_1_to_8_cfg) ) {
			exit(-3);
		}
	}

	/* Create Shutdown Major frame */
	if ( gr1553bc_major_alloc_skel(&final_major, &final_cfg) ) {
		exit(-4);
	}

	printf("Major Frame created successful\n");

/***** CREATE LIST STRUCTURE WITH ALL MAJOR FRAMES *****/

	/* Create List with support for 11 Major frames */
	if ( gr1553bc_list_alloc(&list, 11) ) {
		exit(-10);
	}

	/* Configure list and assign BC device */
	if ( gr1553bc_list_config(list, &gr1553bc_def_cfg, bc) ) {
		printf("Failed to register standard IRQ handler\n");
		exit(-10);
	}

	/* Add Major frames in the right posistion of list */
	if ( gr1553bc_list_set_major(list, initial_major, 0) )
		exit(-11);
	if ( gr1553bc_list_set_major(list, timesync_major, 1) )
		exit(-12);
	for (i=0; i<8; i++)
		if ( gr1553bc_list_set_major(list, comframes[i], i+2) )
			exit(-13-i);
	if ( gr1553bc_list_set_major(list, final_major, 10) )
		exit(-13-8);

	/* The Major frames is now connected be default:
	 * [0] -> [1] -> [2] ... [9] -> [10] -> [0] -> [1]
	 *                                  |- WRAP AROUND
	 *
	 * However we want [0] to only be executed repeativly
	 * until user say initialization isd completed, and 
	 * [10] to be executed on shutdown repeatively until
	 * 1553 is powered down. To manage this we relink
	 * the major frames:
	 *  - [9] to jump to [1]
	 *  - [0] to jump to [0]
	 *  - [10] to jump to [10]
	 *
	 * That means that [0] will be executed infinite until
	 * software makes a jump into [1], then [1]..[9] will
	 * execute in infinite until software make a jump to
	 * [10], which will be executed in infinite until 1553
	 * core is stopped.
	 */
	gr1553bc_list_link_major(initial_major, initial_major);
	gr1553bc_list_link_major(comframes[7], comframes[0]);
	gr1553bc_list_link_major(final_major, final_major);

	printf("List created successful\n");

/***** CREATE DESCRIPTOR TABLE FROM LIST STRUCTURE *****/

	/* Allocate a descriptor table that fits all Major frames */
	if ( gr1553bc_list_table_alloc(list, BD_TABLE_BASE) )
		exit(-30);

	/* Initialize the descriptor table entries:
	 *  - create Jumps between minor frames and between major frames
	 *  - initialize minor syncpoints (will have no IRQ)
	 *  - initialize time slot management end time entry
	 *  - make message slots empty - no bus access.
	 */
	if ( gr1553bc_list_table_build(list) )
		exit(-31);

/***** TEST TRANSFER SLOT ALLOC/FREE *****/
/*
	if ( test_max_slot_alloc() ) {
		exit(-40);
	}

	if ( test_max_slot_free() ) {
		exit(-40);
	}
*/
/***** SETUP INITIAL TRANSFERS *****/

	/* Prepare one slot for jumping to next Major frame (to first minor
	 * in that major frame). Get out of initialization mode.
	 */
	mid = GR1553BC_ID(0, 0, 30);
	gr1553bc_slot_alloc(
		list,
		&mid,
		0,
		NULL);
	gr1553bc_slot_jump(
		list,
		mid,
		GR1553BC_UNCOND_NOJMP,
		GR1553BC_MINOR_ID(1, 0));

	/* Set up 2 Startup broadcast messages to all RTs, to subaddress 1.
	 */
	mid = GR1553BC_ID(0, 0, 10);
	gr1553bc_slot_alloc(
			list,
			&mid,
			20000,	/* 20ms */
			NULL);
	gr1553bc_slot_transfer(
			list,
			mid,
			GR1553BC_OPTIONS_BUSA,
			GR1553BC_BC_BC2RT(1, 1),
			TRANSLATE(&bc_startup_messageA[0])
			);
	mid = GR1553BC_ID(0, 0, 20);
	gr1553bc_slot_alloc(
			list,
			&mid,
			20000,	/* 20ms */
			NULL);
	gr1553bc_slot_transfer(
			list,
			mid,
			GR1553BC_OPTIONS_BUSB,
			GR1553BC_BC_BC2RT(1, 1),
			TRANSLATE(&bc_startup_messageB[0])
			);

	/* Request RT status. Only address 1..9 are used in this example */
	for (i=1; i<10; i++) {

		/* Bus A status requests to RT 1..9*/
		mid = GR1553BC_ID(0, 1, i);
		gr1553bc_slot_alloc(
				list,
				&mid,
				1000,	/* 1ms */
				NULL);
		gr1553bc_slot_transfer(
				list,
				mid,
				GR1553BC_OPTIONS_BUSA,
				GR1553BC_RT2BC(i, 1, 1),
				TRANSLATE(&rt_statusA[i])
				);

		/* Bus B status requests to RT 1..9 */
		mid = GR1553BC_ID(0, 1, 10+i);
		gr1553bc_slot_alloc(
				list,
				&mid,
				1000,	/* 1ms */
				NULL);
		gr1553bc_slot_transfer(
				list,
				mid,
				GR1553BC_OPTIONS_BUSB,
				GR1553BC_RT2BC(i, 1, 1),
				TRANSLATE(&rt_statusB[i])
				);

		/* Bus A Plug&Play info request, RT 0..9 
		 * This info is constant, and the dummy bit
		 * is turned on at the moment, the dummy bit is
		 * cleared when a RT has responded to the status
		 * request on both BusA and BusB above.
		 */
		mid = GR1553BC_ID(0, 2, i);
		gr1553bc_slot_alloc(
				list,
				&mid,
				1000,	/* 1ms */
				NULL);
		gr1553bc_slot_transfer(
				list,
				mid,
				GR1553BC_OPTIONS_BUSA_DUM,
				GR1553BC_RT2BC(i, 2, 16),
				(uint16_t *)TRANSLATE(&rt_pnp_info[i])
				);
	}

	/* Insert an IRQ before we wait for next Major frame */
	mid = GR1553BC_ID(0, 0, 31);
	gr1553bc_slot_alloc(
			list,
			&mid,
			20000,	/* 20ms */
			NULL);
	gr1553bc_slot_irq_prepare(
		list,
		mid,
		list_entry_isr,
		(void *)mid);
	gr1553bc_slot_irq_enable(list, mid);

/***** SETUP TIMESYNC MAJOR FRAME *****/

	/* Extra Time Sync before entering communication state
	 * on major frame 1, executed only once:
	 *
	 * SLOTS:
	 *  (0): WAIT EXT TRIGGER (TIME TICK SYNC), NO BUS OPERATION
	 *  (1): SEND TIME MESSAGE
	 *  (2): UNUSED (MAY BE USED AS TRANSFER)
	 */
	for (i=0; i<2; i++) {
		mid = GR1553BC_ID(1, 0, i);
		gr1553bc_slot_alloc(
			list,
			&mid,
			0,
			NULL);
	}
	gr1553bc_slot_exttrig(
		list,
		GR1553BC_ID(1, 0, 0));
	gr1553bc_slot_transfer(
		list,
		mid,
		GR1553BC_OPTIONS_BUSA,
		GR1553BC_BC_MC_BC2RT(17),
		TRANSLATE(&bc_timesync_nexttime)
		);

/***** SETUP COMMUNICATION FRAMES *****/

	/* Set up Time message in Communication Frame 7, it will guarantuee
	 * that all RTs will have next time at least 50ms before
	 * the "time sync" message is broad casted.
	 *
	 * The time message is used to by the RTs to prepare for the next
	 * time sync message. The preparation may involve distibuting the
	 * time further down the system to it's Time Slaves.
	 *
	 * Major Com Frame 7, Minor frame 6, Slot unknown. for Bus A
	 * Major Com Frame 7, Minor frame 6, Slot unknown. for Bus B
	 * This guatantuee that the new time appear at least 50ms before
	 * time sync message.
	 */
	mid = GR1553BC_MINOR_ID(2+7, 6);
	gr1553bc_slot_alloc(
			list,
			&mid,
			100,	/* 0.1ms */
			NULL);
	gr1553bc_slot_transfer(
			list,
			mid,
			GR1553BC_OPTIONS_BUSA,
			GR1553BC_BC_MC_BC2RT(17),
			TRANSLATE(&bc_timesync_nexttime)
			);
	mid = GR1553BC_MINOR_ID(2+7, 6);
	gr1553bc_slot_alloc(
			list,
			&mid,
			100,	/* 0.1ms */
			NULL);
	gr1553bc_slot_transfer(
			list,
			mid,
			GR1553BC_OPTIONS_BUSB,
			GR1553BC_BC_MC_BC2RT(17),
			TRANSLATE(&bc_timesync_nexttime)
			);

	/* Set up Communication Sync points. We let the BC core generate
	 * IRQ each time a communication Frame sync.
	 * 
	 * Note that the first Communication Frame sync for FRAME0 is 
	 * replaced with a time sync message
	 *
	 */

	/* Insert a Wait Trigger (wait for On-Board Time sync) */
	mid = GR1553BC_ID(2, 0, 0);
	gr1553bc_slot_alloc(list, &mid, 0, NULL);
	gr1553bc_slot_exttrig(list, mid);
	/* "Time Sync no data" is needed to tell RTs to sync
	 * their time with previous time transmitted.
	 */
	mid = GR1553BC_ID(2, 0, 1);
	gr1553bc_slot_alloc(list, &mid, 50, NULL);
	gr1553bc_slot_transfer(
		list,
		mid,
		GR1553BC_OPTIONS_BUSA,
		GR1553BC_BC_MC_NODATA(1),
		NULL
		);
	/* Insert an IRQ point just after so that we know ourselves 
	 * which communication frame we are in.
	 */
	mid = GR1553BC_ID(2, 0, 2);
	gr1553bc_slot_alloc(list, &mid,	0, NULL);
	gr1553bc_slot_irq_prepare(
		list,
		mid,
		bc_com_frame_sync,
		(void *)mid
		);
	gr1553bc_slot_irq_enable(list, mid);

	for (i=1; i<8; i++) {
		mid = GR1553BC_ID(2+i, 0, 0);
		gr1553bc_slot_alloc(list, &mid, 50, NULL);
		/* Communication Frame Sync. Data tells RT which 
		 * Major Frame is executing.
		 */
		gr1553bc_slot_transfer(
			list,
			mid,
			GR1553BC_OPTIONS_BUSA,
			GR1553BC_BC_MC_BC2RT(17),
			TRANSLATE(&bc_frmsync[i])
			);

		/* Insert an IRQ point just after so that we know ourselves 
		 * which communication frame we are in.
		 */
		mid = GR1553BC_ID(2+i, 0, 1);
		gr1553bc_slot_alloc(list, &mid, 0, NULL);
		gr1553bc_slot_irq_prepare(
			list,
			mid,
			bc_com_frame_sync,
			(void *)mid
			);
		gr1553bc_slot_irq_enable(list, mid);
	}

	/* Prepare one slot for jumping to shutdown Major frame (to first minor
	 * in that major frame). Get out of initialization mode.
	 */
	mid = GR1553BC_ID(2+7, 0, 3);
	gr1553bc_slot_alloc(
		list,
		&mid,
		0,
		NULL);
	gr1553bc_slot_jump(
		list,
		mid,
		GR1553BC_UNCOND_NOJMP,
		GR1553BC_MINOR_ID(10, 0));

/***** SETUP SHUTDOWN FRAME *****/

	/* Allocate 2 transfer, don't care about order. 
	 * Send broadcast shutdown message repeativly on
	 * both buses, to sub address 1, 4 words (8byte).
	 */
	mid = GR1553BC_MINOR_ID(10, 0);
	gr1553bc_slot_alloc(
			list,
			&mid,
			2000,	/* 2ms */
			NULL);
	gr1553bc_slot_transfer(
			list,
			mid,
			GR1553BC_OPTIONS_BUSA,
			GR1553BC_BC_BC2RT(1, 1),
			TRANSLATE(&bc_shutdown_messageA[0])
			);
	mid = GR1553BC_MINOR_ID(10, 0);
	gr1553bc_slot_alloc(
			list,
			&mid,
			2000,	/* 2ms */
			NULL);
	gr1553bc_slot_transfer(
			list,
			mid,
			GR1553BC_OPTIONS_BUSB,
			GR1553BC_BC_BC2RT(1, 1),
			TRANSLATE(&bc_shutdown_messageB[0])
			);

	return 0;
}

/* Test time allocation and slot allocation */
int test_max_slot_alloc(void)
{
	int i;
	int mid, status, timefree;
	union gr1553bc_bd *bd;

	/* Try allocating all slots, then an error is expected at 
	 * the next slot. This tests max slot allocation case.
	 */
	for (i=0; i<initial_cfg.minor_cfgs[0].slot_cnt+1; i++) {
		mid = GR1553BC_MINOR_ID(0, 0);
		status = gr1553bc_slot_alloc(
			list,
			&mid,
			1000 + i*100,
			&bd);
		if ( status ) {
			printf("%d: Failed to allocate new slot\n", i);
			/* Expect failure for slot 32 */
			if ( i != 32 )
				return -1;
		} else {
			printf("%d: GOT [%d,%d,%d] BD: %p\n",
				i,
				GR1553BC_MAJID_FROM_ID(mid),
				GR1553BC_MINID_FROM_ID(mid),
				GR1553BC_SLOTID_FROM_ID(mid),
				bd
				);
		}
		mid = GR1553BC_MINOR_ID(0, 0);
		timefree = gr1553bc_list_freetime(list, mid);
		printf("%d: Time left %d [us]\n", i, timefree);
	}
	return 0;
}

int test_max_slot_free(void)
{
	int i;
	int mid, timefree;

	/* Try allocating all slots, then an error is expected at 
	 * the next slot. This tests max slot allocation case.
	 */
	for (i=0; i<initial_cfg.minor_cfgs[0].slot_cnt; i++) {
		mid = GR1553BC_ID(0, 0, i);
		timefree = gr1553bc_list_freetime(list, mid);
		printf("%d: Time left %d [us]\n", i, timefree);

		mid = GR1553BC_ID(0, 0, i);
		timefree = gr1553bc_slot_free(list, mid);
		printf("%d: Freed %d time\n", i, timefree);
	}
	mid = GR1553BC_MINOR_ID(0, 0);
	timefree = gr1553bc_list_freetime(list, mid);
	printf("%d: Time left %d [us]\n", i, timefree);
	return 0;
}

#define NUMBER_OF_RTS 1
int bc_state = 0;
unsigned int rtup = 0;
unsigned int rtpnp = 0;
int state_tick_cnt;

static int dummy_comfrm_cnt = 0;
int dummy_com_frame(int tick)
{
	return tick + (dummy_comfrm_cnt++);
}

struct bc_rt_data_s {
	uint16_t txbufs[8][2][32];
	uint16_t rxbufs[8][2][32];

	int enabled;
	int first;
	int txstate;
	int rxstate;
};

struct bc_rt_data_s bc_rt_data[10] __attribute__ ((aligned (16)));
int sync_errs = 0;

void bc_rt_data_init()
{
	memset(bc_rt_data, 0, sizeof(bc_rt_data));
}

/* Generate Data to be transmitted to RT */
void bc_rt_data_request(uint16_t *buf, int cnt, int *state)
{
	int data = *state;
	int i;

	for (i=0; i<cnt; i++) {
		buf[i] = data & 0xffff;
		data++;
	}

	*state = data;
}

/* Check received data from RT */
int bc_rt_data_check(uint16_t *buf, int cnt, int *state)
{
	int data = *state;
	int i;

	for (i=0; i<cnt; i++) {
		if ( buf[i] != (data & 0xffff) ) {
			printk("Data Received from RT is wrong.\n");
			printk(" %d: Received 0x%04x expected 0x%04x (%x)\n", i, buf[i], 
				data & 0xffff, &buf[i]);
			/* SYNC */
			sync_errs++;
			data = buf[i];
		}
		data++;
	}

	*state = data;

	return 0;
}

/* Set up data transfer */
void bc_rt_data_prepare(int rt)
{
	int i, j;

	if ( (rt == 0) || (rt > 9) ) {
		printf("bc_rt_data_prepare: invalid RT adress\n");
		exit(-1);
	}

	memset(bc_rt_data[rt].txbufs, 0, sizeof(bc_rt_data[rt].txbufs));
	memset(bc_rt_data[rt].rxbufs, 0, sizeof(bc_rt_data[rt].rxbufs));

	/* Let RTs start with different data content */
	bc_rt_data[rt].txstate = rt << 8;
	bc_rt_data[rt].rxstate = rt << 8;

	bc_rt_data[rt].first = 1;
	bc_rt_data[rt].enabled = 1;

	for ( i=0; i<8; i++) {
		for (j=0; j<2; j++) {
			bc_rt_data_request(&bc_rt_data[rt].txbufs[i][j][0], 32,
				&bc_rt_data[rt].txstate);
		}
	}
}

/* Called between each major communication frame number. Handle all
 * RTs data transfers here.
 *
 * Received Data must be checked against transmitted. New data to be
 * transmitted must be generated.
 */
void bc_rt_data_isr(int comfrm)
{
	int i, j, comfrm_last;

	/* Process transfers one once per two Major frames. Remember,
	 * this is just an example :)
	 */
	for (i=1; i<10; i++) {
		/* Process one RT */
		if ( bc_rt_data[i].enabled == 0 )
			continue;
		if ( bc_rt_data[i].first == 1 ) {
			bc_rt_data[i].first = 0;
			continue;
		}

		/* Get last comframe number */
		comfrm_last = comfrm - 1;
		if ( comfrm_last < 0 )
			comfrm_last = 7;

		/* Process last comframe's data transfers */
		for (j=0; j<2; j++) {
			/* Check data */
			bc_rt_data_check(
				&bc_rt_data[i].rxbufs[comfrm_last][j][0],
				32,
				&bc_rt_data[i].rxstate);

			/* Clear data so we can check next transmission */
			memset(&bc_rt_data[i].rxbufs[comfrm_last][j][0], 0, 64);

			/* Generate new data for this comframe */
			bc_rt_data_request(
				&bc_rt_data[i].txbufs[comfrm_last][j][0],
				32,
				&bc_rt_data[i].txstate);
		}
	}
}

int bc_list_process()
{
	switch (bc_state) {
	case 0: /* INIT PHASE */
	{
		int availA = 0, availB = 0, avail = 0;
		int mid, i;
		unsigned int stat;
		unsigned int dummy;

		for (i=1; i<10; i++) {

			/* Check RT on BUSA */
			mid = GR1553BC_ID(0, 1, i);
			if ( gr1553bc_slot_update(list, mid, NULL, &stat) ) {
				printf("Error requesting tr A status\n");
				exit(-1);
			}
			if ( ((stat & 0x7) == 0) && ((stat & (1<<31)) == 0) ) {
				/* RT did respond. Check status */
				printf("RT%d BUSA state: %x\n", i, rt_statusA[i]);
				availA |= 1<<i;
			}

			/* Check RT on BUSB */
			mid = GR1553BC_ID(0, 1, i + 10);
			if ( gr1553bc_slot_update(list, mid, NULL, &stat) ) {
				printf("Error requesting tr B status\n");
				exit(-1);
			}
			if ( ((stat & 0x7) == 0) && ((stat & (1<<31)) == 0) ) {
				/* RT did respond. Check status */
				printf("RT%d BUSB state: %x\n", i, rt_statusB[i]);
				availB |= 1<<i;
			}

			/* Request PnP Info if device is available on
			 * bus A and B.
			 */
			mid = GR1553BC_ID(0, 2, i);
			if ( availA & availB & (1<<i) ) {
				/* Enable PnP Request */
				dummy = 0;
				gr1553bc_slot_dummy(list, mid, &dummy);
				/* Write bit31, it is clear be HW when written*/
				stat = 0x80000000;
				gr1553bc_slot_update(list, mid, NULL, &stat);
				avail++;
			} else {
				/* Disable PnP Request */
				dummy = GR1553BC_TR_DUMMY_1;
				gr1553bc_slot_dummy(list, mid, &dummy);			
			}
		}

		/* Start Bus if a certain number of RTs are available 
		 * This behaviour is a bit weird, but it is one an example...
		 */
		if ( avail >= NUMBER_OF_RTS ) {
			bc_state = 1;
			rtup = availA & availB;

			printf("%d number of RTs has been started\n",
				NUMBER_OF_RTS);
			printf("Switching to PnP State\n");
		}

		break;
	}

	case 1: /* PnP State - We have found X number of RTs */
	{
		/* At this point Plug&Play information may be available
		 * for the RTs that have been found.
		 */

		int mid, i, j, status;
		unsigned int stat;

		rtpnp = 0;

		for (i=1; i<10; i++) {

			/* Check that RT is up before asking for PnP info */
			if ( ((1<<i) & rtup) == 0 )
				continue;

			/* Check that All PnP info is available before
			 * taking the next step.
			 */
			mid = GR1553BC_ID(0, 2, i);

			gr1553bc_slot_update(list, mid, NULL, &stat);
			if ( ((stat & 0x7) == 0) && ((stat & (1<<31)) == 0) ) {
				/* PnP info read successfully */
				rtpnp |= 1<<i;
			}
		}

		/* Check that all PnP info has been read successfully before
		 * proceeding.
		 */
		if ( rtpnp == rtup ) {
			/* Print PnP Info */
			for (i=1; i<10; i++) {
				if ( ((1<<i) & rtpnp) == 0 )
					continue;
				rt_pnp_info[i].desc[20-1] = '\0';
				printf("RT%d PnP: %s\n", i, rt_pnp_info[i].desc);
			}

			/* Signal to RTs that we are in run mode */
			mid = GR1553BC_ID(0, 0, 10);
			gr1553bc_slot_update(list, mid, 
				TRANSLATE(&bc_running_messageA[0]), NULL);
			mid = GR1553BC_ID(0, 0, 20);
			gr1553bc_slot_update(list, mid, 
				TRANSLATE(&bc_running_messageB[0]), NULL);

			state_tick_cnt = 0;
			bc_state = 2;
			printf("Switching to Wait Run State\n");

			/* Prepare for Data communication with RTs
			 * now that we know which RTs are available.
			 *
			 * The communication is started when the
			 * communication state is entered.
			 */
			for (i=1; i<10; i++) {
				if ( ((1<<i) & rtpnp ) == 0 )
					continue;
				/* Make every Major frame transmit
				 * and request data two times. (1kB/s per RT)
				 *
				 * The time between TX and RX is at least 
				 * 25ms.
				 *
				 * This code assumes that not all 9 RTs will
				 * be online. In that case the SLOTs configured
				 * will not be enough.
				 */
				for (j=0; j<8; j++) {

					/* Transmit 128 Bytes */
					mid = GR1553BC_MINOR_ID(2+j, 5);
					status = gr1553bc_slot_alloc(
						list,
						&mid,
						600,	/* 0.6ms */
						NULL);
					if ( status < 0 ) {
						printf("Failed allocate SLOT\n");
						exit(-1);
					}
					gr1553bc_slot_transfer(
						list,
						mid,
						GR1553BC_OPTIONS_BUSA,
						GR1553BC_BC2RT(i, 3, 32),
						TRANSLATE(&bc_rt_data[i].txbufs[j][0][0])
						);
					mid = GR1553BC_MINOR_ID(2+j, 5);
					status = gr1553bc_slot_alloc(
						list,
						&mid,
						600,	/* 0.6ms */
						NULL);
					if ( status < 0 ) {
						printf("Failed allocate SLOT\n");
						exit(-1);
					}
					gr1553bc_slot_transfer(
						list,
						mid,
						GR1553BC_OPTIONS_BUSA,
						GR1553BC_BC2RT(i, 3, 32),
						TRANSLATE(&bc_rt_data[i].txbufs[j][1][0])
						);

					/* Receive 128 Bytes */
					mid = GR1553BC_MINOR_ID(2+j, 7);
					status = gr1553bc_slot_alloc(
						list,
						&mid,
#if 0
						695 + j*1,	/* 700us */
#endif
						600,	/* 600us */
						NULL);
					if ( status < 0 ) {
						printf("Failed allocate SLOT\n");
						exit(-1);
					}
					gr1553bc_slot_transfer(
						list,
						mid,
						GR1553BC_OPTIONS_BUSA,
						GR1553BC_RT2BC(i, 3, 32),
						TRANSLATE(&bc_rt_data[i].rxbufs[j][0][0])
						);
					mid = GR1553BC_MINOR_ID(2+j, 7);
					status = gr1553bc_slot_alloc(
						list,
						&mid,
						600,	/* 0.6ms */
						NULL);
					if ( status < 0 ) {
						printf("Failed allocate SLOT\n");
						exit(-1);
					}
					gr1553bc_slot_transfer(
						list,
						mid,
						GR1553BC_OPTIONS_BUSA,
						GR1553BC_RT2BC(i, 3, 32),
						TRANSLATE(&bc_rt_data[i].rxbufs[j][1][0])
						);
				}

				bc_rt_data_prepare(i);
			}
		}
		break;
	}

	case 2: /* Wait until RTs have entered run mode, this is
	         * expected to take 1s, we reach this function 10
		 * times a second.
		 */
		if ( ++state_tick_cnt >= 10 ) {
			int mid;

#ifdef SOFT_EXTTRIG_ENABLE
			/* Clear the time sync, so that the BC will hang until
			 * time sync comes for the first time.
			 */
 			gr1553bc_ext_trig(bc, 0);
#endif

			/* Jump out of init frame and into communication 
			 * frame. The Slot has already been prepared, so
			 * the data pointer is the only word in the descriptor
			 * that is changed.
			 */
			mid = GR1553BC_ID(0, 0, 30);
			gr1553bc_slot_jump(
				list,
				mid,
				GR1553BC_UNCOND_JMP,
				GR1553BC_MINOR_ID(1, 0));

			bc_enter_com_state(rtup);

			bc_state = 3;
			state_tick_cnt = 0;
			printf("Switching to Communication State\n");
		}
		break;

	case 3:	/* Communication Phase */
	{
		int mid;

		/* Trigger external trigger manually by writing to the 
		 * BC trigger memory at a frequency of 1Hz.
		 *
		 * Note that this is normally performed by a on-board
		 * Time sync hardware.
		 */
		state_tick_cnt++;
		if ( state_tick_cnt == 9 ) {
			bc_timesync_nexttime++;
		} else if ( state_tick_cnt >= 10 ) {
			bc_timesync();
#ifdef SOFT_EXTTRIG_ENABLE
			/* Set Trigger time sync memory */
			gr1553bc_ext_trig(bc, 1);
			printf("TSYNC\n");
#endif
			state_tick_cnt = 0;
		}

		/* jump to shut down major frame?  */
		if ( bc_shutdown ) {
			/* Jump out of First Com-frame and into shutdown
			 * frame. The Slot has already been prepared, so
			 * the Condition (false->true) is the only word in
			 * the descriptor that is changed.
			 */
			mid = GR1553BC_ID(2+7, 0, 3);
			gr1553bc_slot_jump(
				list,
				mid,
				GR1553BC_UNCOND_JMP,
				GR1553BC_MINOR_ID(10, 0));

			/* Go into Shutdown mode */
			bc_state = 4;
			break;
		}

		break;
	}

	case 4: /* Shut Down Phase */
	{
		break;
	}

	default:
		break;
	}

	return 0;
}
