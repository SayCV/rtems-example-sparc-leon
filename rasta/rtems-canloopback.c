/* Software Loopback CAN interface on all OC_CAN interfaces
 * available. Uses a VT100 terminal to print out CAN 
 * message stats.
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

#include <rtems/confdefs.h>

/* Configure Driver manager */
#ifdef LEON2
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
#include <string.h>
#include <sched.h>
#include <occan.h>
#include "occan_lib.h"
#include "vt100.h"

/* Include driver configurations and system initialization */
#include "config.c"

#define MAX_CHANNELS 10

//#undef MULTI_BOARD
//#define TASK_RX
//#define TASK_TX

/* 250kbit/s @ 40MHz */

#define BTR0 0x3
#define BTR1 0x7a


/* 138.88bits/s @ 40MHz */
/*#define BTR0 0x07
#define BTR1 0x69
*/
#define SPEED_250K 250000
#undef OCCAN_USE_SPEED
#undef VT100_DEV
#define UPDATE_DELAY 2
#undef FORCE_SEND_ALL_RECEIVED

#define CANTSK_RX_LEN 128
#define CANTSK_TX_LEN 512

rtems_task loopback_task(rtems_task_argument argument);
rtems_task status_task(rtems_task_argument argument);

/* support max MAX_CHANNELS channels */
int channels;                 /* number of detected CAN interfaces available */
occan_t chans[MAX_CHANNELS];  /* filedescriptors to all CAN devices */
rtems_id tasks[MAX_CHANNELS]; /* array of task ids */
rtems_id tvt100;

struct lb_stat {
  volatile unsigned int sent;
  volatile unsigned int received;
  volatile unsigned int rx_err;
  volatile unsigned int tx_err;
};

volatile struct lb_stat stats[MAX_CHANNELS];

/* ========================================================= 
   initialisation */

rtems_task Init(
        rtems_task_argument ignored
)
{
  rtems_name tname;
  int minor;

  rtems_status_code status;
  printf("******** Starting Gaisler OCCAN test ********\n");

	system_init();

  minor=0;
  while( (minor<MAX_CHANNELS) && (chans[minor]=occanlib_open(minor)) ){
    minor++;
  }
  channels=minor;
  
  if ( channels <= 0 ){
    printf("No OC_CAN hardware found. Aborting demo.\n");
    exit(1);
  }
  
  /* Clear statistics */
  memset((void *)stats,0,sizeof(stats));
  
  /* Init VT100 printing task */
  tname = rtems_build_name( 'V', 'T', '1', 'h' );
  status = rtems_task_create(
          tname, 2, RTEMS_MINIMUM_STACK_SIZE * 2,
          RTEMS_DEFAULT_MODES | RTEMS_PREEMPT, 
          RTEMS_DEFAULT_ATTRIBUTES, &tvt100
          );
  if ( status != RTEMS_SUCCESSFUL ){
    printf("Failed to create vt100 task: %d\n",status);
    exit(1);
  }

  status = rtems_task_start(tvt100, status_task, minor);
  if ( status != RTEMS_SUCCESSFUL ){
    printf("Failed to start vt100 task: %d\n",status);
    exit(1);
  }

  /* Create one Loop-Back task per channel 
   *
   */
  for(minor=0; minor<channels; minor++){
    /* Create a task for channel 'minor' */
    tname = rtems_build_name( 'T', 'S', 'K', 'A'+minor );
    status = rtems_task_create(
          tname, 1, RTEMS_MINIMUM_STACK_SIZE * 2,
          RTEMS_DEFAULT_MODES, 
          RTEMS_DEFAULT_ATTRIBUTES, &tasks[minor]
          );
    if ( status != RTEMS_SUCCESSFUL ){
      printf("Failed to create task %d, status: %d\n",minor,status);
      exit(1);
    }
    
    status = rtems_task_start(tasks[minor], loopback_task, minor);
    if ( status != RTEMS_SUCCESSFUL ){
      printf("Failed to start task %d, status: %d\n",minor,status);
      exit(1);
    }
  }
       
  /* Delete Init task */
  rtems_task_delete(RTEMS_SELF);
}
#ifdef TX_ONLY
#define ID_GAISLER 0x2000

static CANMsg msgs[4];

void init_send_messages(void)
{ 
	/* Send 1 STD Message */
	msgs[0].extended = 0;
	msgs[0].rtr = 0;
	msgs[0].sshot = 0;
	msgs[0].id = 10-1;
	msgs[0].len = 4;
	msgs[0].data[0] = 0x2;
	msgs[0].data[1] = 0xc4;
	msgs[0].data[2] = 0x4f;
	msgs[0].data[3] = 0xf2;
	msgs[0].data[4] = 0x23;
		
	/* Send 3 EXT Message */
	msgs[1].extended = 1;
	msgs[1].rtr = 0;
	msgs[1].sshot = 0;
	msgs[1].id = 10-1;
	msgs[1].len = 4+1;
	msgs[1].data[0] = 0x2;
	msgs[1].data[1] = 0xc4;
	msgs[1].data[2] = 0x4f;
	msgs[1].data[3] = 0xf2;
	msgs[1].data[4] = 0x23;
  msgs[1].data[5] = 0xa2;
	
	msgs[2].extended = 1;
	msgs[2].rtr = 0;
	msgs[2].sshot = 0;
	msgs[2].id = 10+880-1;
	msgs[2].len = 8;
	msgs[2].data[0] = 0xaa;
	msgs[2].data[1] = 0xbb;
	msgs[2].data[2] = 0x11;
	msgs[2].data[3] = 0x22;
	msgs[2].data[4] = 'U';
	msgs[2].data[5] = 0x12;
	msgs[2].data[6] = 0xff;
	msgs[2].data[7] = 0x00;

	msgs[3].extended = 1;
	msgs[3].rtr = 0;
	msgs[3].sshot = 0;
	msgs[3].id = (0xff | ID_GAISLER)-1;
	msgs[3].len = 7;
	msgs[3].data[0] = 'G';
	msgs[3].data[1] = 'a';
	msgs[3].data[2] = 'i';
	msgs[3].data[3] = 's';
	msgs[3].data[4] = 'l';
	msgs[3].data[5] = 'e';
	msgs[3].data[6] = 'r';
}
#endif
/* =========================================================  
 *  CAN loop-back Task.
 */
#ifndef TX_ONLY
	CANMsg canmsgs[50];
#endif

rtems_task loopback_task(
        rtems_task_argument unused
) 
{
  int minor = (int)unused;
	occan_t chan = chans[minor];
	int i=0,j=0,sent,cnt;
#ifdef TX_ONLY
  int left,retry;
#endif
  volatile struct lb_stat *stat = &stats[minor];
	
	printf("Starting CAN loop-back task %d\n",minor);
#ifdef TX_ONLY
  init_send_messages();
#endif
  
	/* before starting set up 
	 *  ¤ Speed
	 *  ¤ Buffer length
	 *  ¤ no Filter (accept all messages)
	 *  ¤ RX and TX blocking mode
	 */
	
	printf("Task%d: Setting speed\n",minor); 
#ifdef OCCAN_USE_SPEED
	occanlib_set_speed(chan,SPEED_250K);
#else
	occanlib_set_btrs(chan,BTR0,BTR1);
#endif
	
	printf("Task%d: Setting buf len: Rx: %d, Tx: %d\n",minor,CANTSK_RX_LEN,CANTSK_TX_LEN); 
	occanlib_set_buf_length(chan,CANTSK_RX_LEN,CANTSK_TX_LEN);
	
	/* total blocking mode */
	printf("Task%d: Setting Rx and Tx blocking mode\n",minor); 
	occanlib_set_blocking_mode(chan,0,1);
	
	/* Set accept all filter */
	/* occanlib_set_filter(occan_t chan, struct occan_afilter *new_filter);*/
	
	/* Start link */
	printf("Task%d: Starting\n",minor); 
	occanlib_start(chan);

	printf("Task%d: Entering CAN Loop-Back loop\n",minor);
	while(1){
  
#ifdef TX_ONLY    
    cnt = 4;
    retry = 0;
    left=cnt;
    while( left > 0 ){
        sent=occanlib_send_multiple(chan,canmsgs+(cnt-left),left);
        if ( sent < 0 ) {
          stat->tx_err++;
          printf("Task%d: Experienced TX error\n",minor);
          retry++;
          if ( retry >= 3 )
            break;
          /*sleep(1);*/
        }else{
          /* Update stats */
          stat->sent += sent;
          left-=sent;
        }
		}
    /*sleep(1);*/
    
    continue;
#endif

		/* blocking read */
		cnt = occanlib_recv_multiple(chan,canmsgs,10);
		if ( cnt > 0 ){
			/*printf("Task%d: Got %d messages\n",minor,cnt);*/
      stat->received += cnt;
      
      /* Signal to Status task that the staticstics has been updated */

      /* Change ID of message
       * This is because there can be an arbitration conflict 
       * if the same id is sent and received at the same time.
       *
       * inrement ID once
       */
      for(i=0; i<cnt; i++){
        canmsgs[i].id+=(((unsigned int)'u')<<10);
        canmsgs[i].id--;
        
        for(j=0; j<canmsgs[i].len; j++)
          canmsgs[i].data[j]++;
        
      }
#ifndef FORCE_SEND_ALL_RECEIVED
/* Just try send if possible */
      sent=occanlib_send_multiple(chan,canmsgs,cnt);
      if ( sent>0 )
        stat->sent += sent;
#else
      /* Reply message */
      retry=0;
      left=cnt;
      while( left > 0 ){
        sent=occanlib_send_multiple(chan,canmsgs+(cnt-left),left);
        if ( sent < 0 ) {
          stat->tx_err++;
          printf("Task%d: Experienced TX error\n",minor);
          retry++;
          if ( retry >= 3 )
            break;
          sleep(1);
        }else{
          /* Update stats */
          stat->sent += sent;
          left-=sent;
          retry=0;
        }
			}
#endif
		}else if ( cnt < 0) {
			printf("Task%d: Experienced RX error\n",minor);
      stat->rx_err++;
      sleep(1);
		}else{
			/* if in non-blocking mode we work with other stuff here */
      printf("Task%d: RX blocking not working\n",minor);
			sleep(1);
		}
	}
  
  /* Delete this task */
  rtems_task_delete(RTEMS_SELF);
}

typedef struct {
	unsigned char 
		mode,
		cmd,
		status,
		intflags,
		inten,
		resv0,
		bustim0,
		bustim1,
		unused0[2],
		resv1,
		arbcode,
		errcode,
		errwarn,
		rx_err_cnt,
		tx_err_cnt,
		rx_fi_xff; /* this is also acceptance code 0 in reset mode */ 
		union{
			struct {
				unsigned char id[2];
				unsigned char data[8];
				unsigned char next_in_fifo[2];
			} rx_sff;
			struct {
				unsigned char id[4];
				unsigned char data[8];
			} rx_eff;
			struct {
				unsigned char id[2];
				unsigned char data[8];
				unsigned char unused[2];
			} tx_sff;
			struct {
				unsigned char id[4];
				unsigned char data[8];
			} tx_eff;
			struct {
				unsigned char code[3];
				unsigned char mask[4];
			} rst_accept;
		} msg;
		unsigned char rx_msg_cnt;
		unsigned char unused1;
		unsigned char clkdiv;
} pelican_regs;

 #define READ_REG(address) _tmp_REG_READ((unsigned int)address)
 unsigned char _tmp_REG_READ(unsigned int addr) {
        unsigned char tmp;
        asm(" lduba [%1]1, %0 "
            : "=r"(tmp)
            : "r"(addr)
           );
        return tmp;
	}


void print_stat(int chan)
{ 
  occan_stats canstats;
  volatile struct lb_stat *s = &stats[chan];
  
  pelican_regs *regs = (void *)(0xfffc0000+0x100*chan);
  /*vt100_printf(chan+4,1,"|  %-7d| %-11d| %-11d| %-12d| %-12d|",chan,s->received,s->sent,s->rx_err,s->tx_err);*/
  vt100_printf(chan*2+4,1,"|  %-7d| %-11d| %-11d| %-12d| %-12d|",chan,s->received,s->sent,READ_REG(&regs->rx_err_cnt),READ_REG(&regs->tx_err_cnt));
  sched_yield();
  if ( occanlib_get_stats(chans[chan],&canstats) ){
    vt100_printf(chan*2+5,1,"|  %-7d| %-11d| %-11d| %-12d| %-12d|",chan,-1,-1,-1,-1);
  }else{
    vt100_printf(chan*2+5,1,"|  %-7d| %-11d| %-11d| %-12d| %-12d|",chan,canstats.err_dovr,canstats.err_errp,canstats.rx_sw_dovr,canstats.rx_msgs);
  }
  
  
}

void print_stats(int chan_cnt)
{
  int i;
  vt100_reset();
  vt100_clear();
  sched_yield();
  vt100_printf(1,1,"/---------------------------------------------------------------\\");
  sched_yield();
  vt100_printf(2,1,"| Channel |  RX count  |  TX count  | RxErr count | TxErr Count |");
  sched_yield();
  vt100_printf(3,1,"|---------------------------------------------------------------|");
  vt100_flush();
  sched_yield();
  for(i=0;i<chan_cnt;i++){
    print_stat(i);
    sched_yield();
  }
  vt100_printf(chan_cnt*2+4,1,"\\---------------------------------------------------------------/");
  sched_yield();
    
  vt100_flush();
}

rtems_task status_task(rtems_task_argument argument)
{
  struct lb_stat local_stats[MAX_CHANNELS];
  
  sleep(5);
  
#ifdef VT100_DEV
  {
    FILE *stream;
    
    /* Open serial devcice */
    stream = fopen(VT100_DEV,O_RDWR);
    
    /* if fopen failed vt100_init will select stdout for us */
    vt100_init(stream);
  }
#else
  /* Use Standard output */
  vt100_init(NULL);
#endif

  memset(local_stats,0,sizeof(local_stats));  
  
  print_stats(channels);
    
  while(1){
    
    sleep(UPDATE_DELAY);
    
    print_stats(channels);

  }
}
