/* Simple OC_CAN interface test.
 *
 * Gaisler Research 2007,
 * Daniel Hellström
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

/* Configure RTEMS */
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
#define CONFIGURE_DRIVER_AMBAPP_GAISLER_PCIF    /* PCI is for GR-701 OCCAN */
#define CONFIGURE_DRIVER_AMBAPP_GAISLER_GRPCI   /* PCI is for GR-701 OCCAN */
#define CONFIGURE_DRIVER_PCI_GR_701             /* GR-701 PCI TARGET has a OCCAN core */

#ifdef LEON2
  /* PCI support for AT697 */
  #define CONFIGURE_DRIVER_LEON2_AT697PCI
  /* AMBA PnP Support for GRLIB-LEON2 */
  #define CONFIGURE_DRIVER_LEON2_AMBAPP
#endif

#define CONFIGURE_DRIVER_AMBAPP_GAISLER_OCCAN   /* OCCAN Driver */

#include <drvmgr/drvmgr_confdefs.h>

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <occan.h>
#include "occan_lib.h"


/* Include driver configurations and system initialization */
#include "config.c"


#define BTR0 0x27	
#define BTR1 0x3e
#define DO_FILTER_TEST	

//#undef MULTI_BOARD
//#define TASK_RX
//#define TASK_TX

#ifdef MULTI_BOARD
/* Only one CAN core is used either as TX or RX */
#define OCCAN_DEVICE_TX_NAME "/dev/occan0"
#define OCCAN_DEVICE_RX_NAME "/dev/occan0"
#else
/* Loopback mode - both CAN cores are used one for RX and one for TX */
#define OCCAN_DEVICE_TX_NAME "/dev/occan0"
#define OCCAN_DEVICE_RX_NAME "/dev/occan1"
#endif


rtems_task task1(rtems_task_argument argument);
rtems_task task2(rtems_task_argument argument);

/*extern int errno;*/

rtems_id   Task_id[3];         /* array of task ids */
rtems_name Task_name[3];       /* array of task names */

/* ========================================================= 
   initialisation */

rtems_task Init(
        rtems_task_argument ignored
)
{
        rtems_status_code status;

        /* Initialize Driver manager, in config.c */
        system_init();

        printf("******** Starting Gaisler OCCAN sample ********\n");

        Task_name[1] = rtems_build_name( 'T', 'S', 'K', 'A' );
        Task_name[2] = rtems_build_name( 'T', 'S', 'K', 'B' );

        status = rtems_task_create(
                Task_name[1], 1, RTEMS_MINIMUM_STACK_SIZE * 2,
                RTEMS_DEFAULT_MODES, 
                RTEMS_DEFAULT_ATTRIBUTES, &Task_id[1]
                );

        status = rtems_task_create( 
                Task_name[2], 1, RTEMS_MINIMUM_STACK_SIZE * 2,
                RTEMS_DEFAULT_MODES,
                RTEMS_DEFAULT_ATTRIBUTES, &Task_id[2]
                );

        /* Starting receiver first */
#ifdef TASK_RX
        status = rtems_task_start(Task_id[2], task2, 2);
#endif
#ifdef TASK_TX
        status = rtems_task_start(Task_id[1], task1, 1);
#endif
        status = rtems_task_delete(RTEMS_SELF);
}



void task1_afilter_test(occan_t chan);
void task2_afilter_test(occan_t chan);

#define SPEED_250K 250000

#define TSK1_RX_LEN 32
#define TSK1_TX_LEN 8

#define TSK2_RX_LEN 32
#define TSK2_TX_LEN 8

#define ID_GAISLER 0x2000

/* =========================================================  
   task1 */
	
rtems_task task1(
        rtems_task_argument unused
) 
{
	occan_t chan;
	CANMsg msgs[3];
	int i=0,j=0,left,sent;
	occan_stats stats;
	
	printf("Starting task 1\n");
		
	/* open device */
	chan=occanlib_open(OCCAN_DEVICE_TX_NAME);
	if ( !chan ){
		printf("Failed to open device driver 0\n");
		rtems_task_delete(RTEMS_SELF);
		return;
	}

#ifdef DO_FILTER_TEST
	/* Do filter test */
	task1_afilter_test(chan);
	
	occanlib_stop(chan);
#endif
	/* before starting set up 
	 *  ¤ Speed
	 *  ¤ Buffer length
	 *  ¤ no Filter 
	 *  ¤ blocking mode
	 */
	
	printf("Task1: Setting speed\n"); 
	//occanlib_set_speed(chan,SPEED_250K);
        occanlib_set_btrs(chan,BTR0,BTR1);
	
	printf("Task1: Setting buf len\n"); 
	occanlib_set_buf_length(chan,TSK1_RX_LEN,TSK1_TX_LEN);
	
	/* total blocking mode */
	printf("Task1: Setting blk mode\n"); 
	occanlib_set_blocking_mode(chan,1,1);	
	
	/* Set accept all filter */
	/* occanlib_set_filter(occan_t chan, struct occan_afilter *new_filter);*/
	
	/* Start link */
	printf("Task1: Starting\n"); 
	occanlib_start(chan);

	printf("Task1: Entring TX loop\n");
	while(1){
		/* Send 1 STD Message */
		msgs[0].extended = 0;
		msgs[0].rtr = 0;
		msgs[0].sshot = 0;
		msgs[0].id = 10+i;
		msgs[0].len = 4 + (i&1);
		msgs[0].data[0] = 0x2;
		msgs[0].data[1] = 0xc4;
		msgs[0].data[2] = 0x4f;
		msgs[0].data[3] = 0xf2;
		msgs[0].data[4] = 0x23;
		
		printf("Task1: sending 1 STD msg\n");
		occanlib_send(chan,&msgs[0]);
		
		/* Send 3 EXT Message */
		msgs[0].extended = 1;
		msgs[0].rtr = 0;
		msgs[0].sshot = 0;
		msgs[0].id = 10+i;
		msgs[0].len = 4 + (i&1);
		msgs[0].data[0] = 0x2;
		msgs[0].data[1] = 0xc4;
		msgs[0].data[2] = 0x4f;
		msgs[0].data[3] = 0xf2;
		msgs[0].data[4] = 0x23;
		
		msgs[1].extended = 1;
		msgs[1].rtr = 0;
		msgs[1].sshot = 0;
		msgs[1].id = 10+i;
		msgs[1].len = 8;
		msgs[1].data[0] = 0xaa;
		msgs[1].data[1] = 0xbb;
		msgs[1].data[2] = 0x11;
		msgs[1].data[3] = 0x22;
		msgs[1].data[4] = 'U';
		msgs[1].data[5] = 0x12;
		msgs[1].data[6] = 0xff;
		msgs[1].data[7] = 0x00;

		msgs[2].extended = 1;
		msgs[2].rtr = 0;
		msgs[2].sshot = 0;
		msgs[2].id = (10+i) | ID_GAISLER;
		msgs[2].len = 7;
		msgs[2].data[0] = 'G';
		msgs[2].data[1] = 'a';
		msgs[2].data[2] = 'i';
		msgs[2].data[3] = 's';
		msgs[2].data[4] = 'l';
		msgs[2].data[5] = 'e';
		msgs[2].data[6] = 'r';
						
	  /* Send 3 EXT Messages */
		printf("Task1: sending 3 EXT msg\n");
		left = 3;
		while ( left > 0 ){
			sent = occanlib_send_multiple(chan,&msgs[3-left],left);
			if ( sent < 0 ){
				printf("Task1: Error, aborting 3 EXT sending\n");
			}else{
				left -= sent;
			}
		}
		
		j++;
		if ( j == 10 ){
			/* debug start/stop */
			printf("---------------- Task1: Stopping ----------------\n"); 
			occanlib_stop(chan);
			
			printf("---------------- Task1: Starting again --------------\n"); 
			occanlib_start(chan);
		}
		
		if ( (j & 0x1f) == 0x1f ){
			printf("---------------- Task1: Printing Stats --------------\n"); 
			if ( !occanlib_get_stats(chan,&stats) ){			
				occanlib_stats_print(&stats);
			}
			printf("---------------- Task1: done Printing  --------------\n"); 
		}

					
		/* pause for 1 sec */
		sleep(1);
		i++;
		if ( i>240 )
			i=0;
	}
}

/* ========================================================= 
   task2 */

rtems_task task2(
        rtems_task_argument unused
) 
{
	occan_t chan;
	CANMsg msgs[3];
	int i,cnt,msgcnt;
	struct occan_afilter afilt;
	
	printf("Starting task 2\n");
	
	/* open device */
	chan=occanlib_open(OCCAN_DEVICE_RX_NAME);
	if ( !chan ){
		printf("Failed to open device driver 1\n");
		rtems_task_delete(RTEMS_SELF);
		return;
	}
#ifdef DO_FILTER_TEST
	/* do filter test 2 */
	task2_afilter_test(chan);
	
	occanlib_stop(chan);
#endif
	/* before starting set up 
	 *  ¤ Speed
	 *  ¤ Buffer length
	 *  ¤ no Filter 
	 *  ¤ blocking mode
	 */
	
	printf("Task2: Setting speed\n"); 
	//occanlib_set_speed(chan,SPEED_250K);
	occanlib_set_btrs(chan,BTR0,BTR1);
  
	printf("Task2: Setting buf len\n"); 
	occanlib_set_buf_length(chan,TSK2_RX_LEN,TSK2_TX_LEN);
	
	/* total blocking mode */
	printf("Task2: Setting blk mode\n"); 
	occanlib_set_blocking_mode(chan,0,1);
	
	/* Set filter to accept all */
	afilt.single_mode = 1;
	afilt.code[0] = 0x00;
	afilt.code[1] = 0x00;
	afilt.code[2] = 0x00;
	afilt.code[3] = 0x00;
	afilt.mask[0] = 0xff; /* don't care */
	afilt.mask[1] = 0xff;
	afilt.mask[2] = 0xff;
	afilt.mask[3] = 0xff;
	occanlib_set_filter(chan,&afilt);
	
	/* Start link */
	printf("Task2: Starting\n"); 
	occanlib_start(chan);
	
	msgcnt=0;
	printf("Task2: Entering rx loop\n");
	while(2){
		/* blocking read */
		cnt = occanlib_recv_multiple(chan,msgs,3);
		if ( cnt > 0 ){
			printf("Task2: Got %d messages\n",cnt);
			for(i=0; i<cnt; i++){
				if ( msgs[i].id & ID_GAISLER ){
					printf("----- GAISLER MESSAGE -----\n");
				}
				print_msg(msgcnt,&msgs[i]);
				if ( msgs[i].id & ID_GAISLER ){
					printf("---------------------------\n");
				}
				msgcnt++;
			}
		}else if ( cnt < 0) {
			printf("Task2: Experienced RX error\n");
		}else{
			/* if in non-blocking mode we work with other stuff here */
			printf("Task2: waiting 1s\n");
			sleep(1);
		}
	}
}

#ifdef DO_FILTER_TEST	
/************* acceptance filter test *************/
void task1_afilter_test(occan_t chan){
	CANMsg msgs[16];
	int left, sent, i;
	occan_stats stats;

	printf("Task1: Setting speed\n"); 
	occanlib_set_speed(chan,SPEED_250K);
	
	/* total blocking mode */
	printf("Task1: Setting blk mode\n"); 
	occanlib_set_blocking_mode(chan,1,1);
	
	printf("Task1: Setting buf len\n"); 
	occanlib_set_buf_length(chan,TSK1_RX_LEN,TSK1_TX_LEN);
	
	/* Build messages */
	for(i=0; i<16; i++){
		msgs[i].id = 0x1 << i;
		msgs[i].extended = 1;
		msgs[i].rtr = 0;
		msgs[i].sshot = 0;
		msgs[i].len = 1;
		msgs[i].data[0] = i;
		printf("Task1: Message %d: ID: 0x%lx\n",i,msgs[i].id);
	}
	
	/* Start */
	occanlib_start(chan);
	
	printf("Task1: sending 16 EXT msgs\n");
	left = 16;
	while ( left > 0 ){
		sent = occanlib_send_multiple(chan,&msgs[16-left],left);
		if ( sent < 0 ){
			printf("Task1: Error, aborting 16 EXT sending\n");
			break;
		}else{
			left -= sent;
		}
	}
	
	printf("Task1: Exiting (%d)\n",left);
	sleep(4);
	printf("---------------- Task1: Printing Stats --------------\n"); 
	if ( !occanlib_get_stats(chan,&stats) ){
		occanlib_stats_print(&stats);
	}
	printf("---------------- Task1: done Printing  --------------\n"); 
}

void task2_afilter_test(occan_t chan){
	int tries, cnt, tot, i;
	CANMsg msgs[16];
	struct occan_afilter filt;
	occan_stats stats;
	
	printf("Task2: Setting speed\n"); 
	occanlib_set_speed(chan,SPEED_250K);
	
	/* total blocking mode */
	printf("Task2: Setting non-blk mode\n"); 
	occanlib_set_blocking_mode(chan,0,0);
	
	printf("Task2: Setting buf len\n"); 
	occanlib_set_buf_length(chan,TSK1_RX_LEN,TSK1_TX_LEN);
	
	/* Set up filter so that odd messages is filtered out 
	 * It can be done with a single filter.
	 * 
	 * Odd Messages: ID = 1<<i (i=odd) 0xaaaaaaa
	 * Even:         ID = 1<<i (i=even 0,2,4) 0x55555
	 *
	 * All odd bits must be zero:
	 *
	 * Mask = 0x aaaa aaaa
	 * Code = 0x0
	 *
	 */
	filt.single_mode = 1;
	filt.code[0] = 0;
	filt.code[1] = 0;
	filt.code[2] = 0;
	filt.code[3] = 0;
	filt.mask[0] = 0xaa;
	filt.mask[1] = 0xaa;
	filt.mask[2] = 0xaa;
	filt.mask[3] = 0xaf;
	occanlib_set_filter(chan,&filt);
	
	/* Start */
	occanlib_start(chan);
	
	tot=tries=0;
	while(tries < 5){
		/* Read 1 message */
		cnt = occanlib_recv_multiple(chan,msgs,16);
		printf("Task2: Got %d Message(s)\n",cnt);
		
		if ( cnt > 0 ){
			for(i=0; i<cnt; i++){
				if ( msgs[i].data[0] & 1 ){
					printf("Task2: ERROR! GOT ODD MESSAGE\n");
				}
				printf("Task2: MSG %d ID: 0x%lx, Data[0]: %d(0x%x)\n",tot,msgs[i].id,msgs[i].data[0],msgs[i].data[0]);
				tot++;
			}
		}
		
		tries++;
		sleep(1);
	}
	
	if ( tot != 8 ){
		printf("Task2: Total count is wrong: %d, tries: %d\n",tot,tries);
	}
	
	printf("Task2: Exiting filtering test\n");
	printf("---------------- Task2: Printing Stats --------------\n"); 
	if ( !occanlib_get_stats(chan,&stats) ){
		occanlib_stats_print(&stats);
	}
	printf("---------------- Task2: done Printing  --------------\n"); 
	
}
#endif
