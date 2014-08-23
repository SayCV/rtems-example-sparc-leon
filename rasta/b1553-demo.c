/* Simple BRM interface test.
 * Program can be compiled in one of three modes:
 *  ¤ BC - Bus controller
 *  ¤ RT - Remote Terminal
 *  ¤ BM - Bus monitor
 * 
 * Gaisler Research 2007,
 * Daniel Hellström
 *
 */

#include <rtems.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <bsp.h> /* for device driver prototypes */
#include <stdio.h>
#include <stdlib.h>

#include <ambapp.h>
#include <b1553brm.h>
#include "brm_lib.h"

//#define BRM_BC_TEST
//#define BRM_BM_TEST

#define BRM_BC_TEST

rtems_task task1(rtems_task_argument argument);

static rtems_id   tds[2];        /* array of task ids */
static rtems_name tnames[2];     /* array of task names */

static brm_t chan;

/* ========================================================= 
   initialisation */

int b1553_init(void)
{
  rtems_status_code status;
  int i;
  
  printf("******** Initiating RASTA 1553 test ********\n");

  for ( i=0; i<1; i++){
    tnames[i] = rtems_build_name( 'T', 'D', 'B', '0'+i );
  
    status = rtems_task_create(
      tnames[i], 1, RTEMS_MINIMUM_STACK_SIZE * 4,
      RTEMS_DEFAULT_MODES | RTEMS_TIMESLICE,
      RTEMS_DEFAULT_ATTRIBUTES, &tds[i]
    );
    
    if ( status != RTEMS_SUCCESSFUL) {
      printf("Fialed to create B1553 tasks\n");
      return -1;
    }
  }
  
  /* Open B1553 driver */
	chan=brmlib_open("/dev/rastaio0/b1553brm0");
	if ( !chan ){
		printf("Failed to open /dev/rastaio0/b1553brm0 driver (%d)\n",errno);
		return -1;
	}
  		
  return 0;
}

void b1553_start(void)
{
  rtems_status_code status;

  /* Starting receiver first */
  status = rtems_task_start(tds[0], task1, 0);
}

#ifdef BRM_BC_TEST
/* Bus controller */

/* ========================================================= 
   task1 BC */

/* Execution list length  */
#define MSG_CNT 4

int proccess_list(brm_t chan, struct bc_msg *list, int test){
	int j,ret;
	
	if ( brmlib_bc_dolist(chan, list) ){
		printf("LIST EXECUTION INIT FAILED\n");
		return -1;
	}
		
	/* Blocks until done */
	printf("Waiting until list processed\n"); 
	if ( (ret=brmlib_bc_dolist_wait(chan)) < 0 ){
		printf("LIST EXECUTION DONE FAILED\n");
		return -1;
	}
		
	if ( ret != 1 ) {
		/* not done */
		printf("LIST NOT PROCESSED\n");
		return -1;
	}
	/* done */
	printf("List processed.\n");
	
	for (j = 1; j <= MSG_CNT; j++) {
    if (list[j-1].ctrl & BC_BAME)
  		printf("Error msg %d, %x: BAME\n", j, list[j-1].ctrl);
  }
		
	return 0;
}

rtems_task task1(
        rtems_task_argument unused
) 
{
	struct bc_msg cmd_list[MSG_CNT+1];
	struct bc_msg result_list[MSG_CNT+1];
	int j,k;
	
	printf("Starting task 1: BC mode\n");
		
	/* before starting set up
	 *  ¤ blocking mode
	 *  ¤ BC mode
	 */
	
	/* Set BC mode */
	printf("Task1: Setting BC mode\n");
	brmlib_set_mode(chan,BRM_MODE_BC);
	
	/* total blocking mode */
	printf("Task1: Setting TX/RX blocking mode\n"); 
	brmlib_set_block(chan,1,1);
	
	printf("Setting up command list.\n");
	
	/* Begin execution list loop */
	while (1) {
	
		/* Set up messages to RT receive subaddresses */
		for (j = 1; j <= MSG_CNT; j++) {         
			cmd_list[j-1].rtaddr[0]  = 1;
			cmd_list[j-1].subaddr[0] = j;
			cmd_list[j-1].wc         = 8;
			cmd_list[j-1].ctrl       = BC_BUSA; /* RT receive on bus a */
			for (k = 0; k < 9; k++){
  			cmd_list[j-1].data[k] = 0;
  		}
			/* message input */
			cmd_list[j-1].data[1] = 'G';
			cmd_list[j-1].data[2] = 'R';
			cmd_list[j-1].data[3] = (j-1)+7;
	  }
		cmd_list[MSG_CNT-1].wc++;
		cmd_list[MSG_CNT].ctrl |= BC_EOL;   /* end of list */
	
		/* Set up RT transmit sub addresses (request RTs to send answer) */
		for (j = 1; j <= MSG_CNT; j++) {         
			result_list[j-1].rtaddr[0]  = 1;
			result_list[j-1].subaddr[0] = j;
			result_list[j-1].wc         = 8;
			result_list[j-1].ctrl       = BC_BUSA | BC_TR; /* RT transmit on bus a */		
			/* clear data */
			for (k = 0; k < 9; k++){
  			result_list[j-1].data[k] = 0;
  		}
	  }
		result_list[MSG_CNT-1].wc++;
		result_list[MSG_CNT].ctrl |= BC_EOL;   /* end of list */
	
		printf("-------------  BC: START LIST EXECUTION -------------\n");
	
		printf("Start CMD list processing.\n"); 
    if ( proccess_list(chan,cmd_list,0) ){
			sleep(1);
			continue;
		}
				   
    

		printf("Sleeping 20s\n");
		sleep(20);
		printf("-------------  BC: START LIST EXECUTION -------------\n");
		printf("Start RESULT list processing.\n"); 
		
		/* Clear data that input will overwrite */
		for (j = 1; j <= MSG_CNT; j++) {
			/* clear data */
			for (k = 0; k < 8; k++){
  			result_list[j-1].data[k] = 0;
  		}
  	}
		
    if ( proccess_list(chan,result_list,1) ){
			sleep(1);
			continue;
		}
		
		/* print the data that was received */
		j=1;
		while( !(result_list[j-1].ctrl & BC_EOL) ){
			printf("Response to message %d: (len: %d, tsw1: %x, tsw2: %x)\n  ",j,result_list[j-1].wc,result_list[j-1].tsw[0],result_list[j-1].tsw[1]);
			/* print data */			
			for (k = 0; k < result_list[j-1].wc; k++){
				if ( isalnum(result_list[j-1].data[k]) ){
  				printf("0x%x (%c)",result_list[j-1].data[k],result_list[j-1].data[k]);
				}else{
					printf("0x%x (.)",result_list[j-1].data[k]);
				}
  		}
			printf("\n");
			j++;
  	}
		
		printf("-----------------------------------------------------\n");		
		printf("Sleeping 15s\n");
		sleep(15);
	}
}

#elif defined(BRM_BM_TEST)
/* Bus monitor */
/* =========================================================  
   task1 BM */

rtems_task task1(
        rtems_task_argument unused
) 
{
	struct bm_msg msgs[3];
	int i;
	int ret,cnt;
	int tot;
	
	printf("Starting task 1: BM mode\n");

	/* before starting set up
	 *  ¤ blocking mode
	 *  ¤ BM mode
	 */
	
	/* Set BM mode */
	printf("Task1: Setting BM mode\n");
	brmlib_set_mode(chan,BRM_MODE_BM);
	
	/* total blocking mode */
	printf("Task1: Setting TX and RX blocking mode\n"); 
	brmlib_set_block(chan,1,1);

	tot=0;
	while(1){
		cnt = brmlib_bm_recv_multiple(chan,msgs,3);
		
		/* print messages */
		for (i=0; i<cnt; i++,tot++){
			print_bm_msg(tot,&msgs[i]);
		}
	}
}

#else /* RT mode */
/* =========================================================  
   task1 RT */
#define MSG_CNT 3
rtems_task task1(
        rtems_task_argument unused
) 
{
	struct rt_msg msgs[MSG_CNT];
	int i;
	char buf[1024];
	int ofs,ret;
	int msglen;
	
	printf("Starting task 1: RT mode\n");
  	
	/* before starting set up
	 *  ¤ blocking mode
	 *  ¤ RT mode
	 */
	
	/* Set RT mode */
	printf("Task1: Setting RT mode\n");
	brmlib_set_mode(chan,BRM_MODE_RT);
	
	/* total blocking mode */
	printf("Task1: Setting TX blocking RX non-blocking mode\n"); 
	brmlib_set_block(chan,1,0);	
	
	/* Set RT address (defaults to 1) */
	
	while (1) {
		/* read 1 message a time */
		ret = brmlib_rt_recv(chan,msgs);
		if ( ret <= 0 ){
			/*  */
			printf("Sleeping 1s, %d\n",ret);
			sleep(1);
			continue;
		}
		
		if ( msgs[0].desc >= 32 ){
			printf("Message desc >= 32\n");
			sleep(1);
			continue;
		}
						
		printf("---------------  RT MESSAGE: -------------------\n");
		if (msgs[0].miw & (1<<7)) {
			printf("Message error, desc: %d, miw:%x\n", msgs[0].desc, msgs[0].miw);
    }
		
		printf("desc: %d, miw: %x, time: %x\n",msgs[0].desc, msgs[0].miw, msgs[0].time);
		
		msglen = (msgs[0].miw >> 11) & 0x1f;
		ofs=0;
		for(i=0; i<msglen; i++){
			ofs += sprintf(buf+ofs,"0x%04x ",msgs[0].data[i]);
		}
		printf("Data: %s\n",buf);
		printf("------------------------------------------------\n");
		
		/* reply with twisted data */
		for(i=0; i<msglen; i++)
			msgs[0].data[i] = msgs[0].data[i]+i;

		/* respond to incoming message by putting it into 
		 * transmit sub address.
		 */
		msgs[0].desc += 32;
		if ( brmlib_rt_send(chan,msgs) != 1 ){
			printf("Error replying\n");
		}
	}
}
#endif

void b1553_print_stats(void)
{

}
