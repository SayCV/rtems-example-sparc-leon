/* Simple GRCAN interface example.
 *
 * Two tasks are communicating. Task2 (TX) sends data to
 * Task1 (RX) which verify the data content. Both read RX
 * and TX task collect statistics so that it can be 
 * printed by a call to can_print_stats.
 *
 * In order for the example to work an external board 
 * responding to the messages sent is needed.
 * The example below expects the messages to be sent back
 * with the ID-field decremented once, all other data in 
 * message must be unmodified.
 *
 * The RX task may indicate dropped messages if the external
 * board doesn't send back all sent messages in time.
 * 
 * Gaisler Research 2007,
 * Daniel Hellström
 *
 */

#include <rtems.h>
#define CONFIGURE_INIT
#include <bsp.h> /* for device driver prototypes */

rtems_task Init( rtems_task_argument argument);	/* forward declaration needed */

/* configure RTEMS kernel */
#define CONFIGURE_APPLICATION_NEEDS_CONSOLE_DRIVER
#define CONFIGURE_APPLICATION_NEEDS_CLOCK_DRIVER
#define CONFIGURE_MAXIMUM_TASKS             16
#define CONFIGURE_RTEMS_INIT_TASKS_TABLE
#define CONFIGURE_EXTRA_TASK_STACKS         (24 * RTEMS_MINIMUM_STACK_SIZE)
#define CONFIGURE_LIBIO_MAXIMUM_FILE_DESCRIPTORS 32
#define CONFIGURE_INIT_TASK_PRIORITY	100
#define CONFIGURE_MAXIMUM_DRIVERS 16

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
#define CONFIGURE_DRIVER_AMBAPP_GAISLER_PCIF    /* PCI is for GR-RASTA-IO GRCAN */
#define CONFIGURE_DRIVER_AMBAPP_GAISLER_GRPCI   /* PCI is for GR-RASTA-IO GRCAN */
#define CONFIGURE_DRIVER_PCI_GR_RASTA_IO        /* GR-RASTA-IO PCI TARGET has a GRCAN core */

#ifdef LEON2
  /* PCI support for AT697 */
  #define CONFIGURE_DRIVER_LEON2_AT697PCI
  /* AMBA PnP Support for GRLIB-LEON2 */
  #define CONFIGURE_DRIVER_LEON2_AMBAPP
#endif

#define CONFIGURE_DRIVER_AMBAPP_GAISLER_GRCAN   /* GRCAN Driver */

#include <drvmgr/drvmgr_confdefs.h>

#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sched.h>
#include <ctype.h>
#include <bsp.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>

/* Include driver configurations and system initialization */
#include "config.c"

#include <grcan.h>

/* Select CAN core to be used in sample application.
 *  - /dev/grcan0              (First ON-CHIP core)
 *  - /dev/grcan1              (Second ON-CHIP core)
 *  - /dev/rastaio0/grcan0     (The GRCAN core on first GR-RASTA-IO board)
 *  - /dev/rastaio1/grcan0     (The GRCAN core on second GR-RASTA-IO board)
 */
#define GRCAN_DEVICE_NAME "/dev/grcan0"
/*#define GRCAN_DEVICE_NAME "/dev/rastaio0/grcan0"*/

/* Define this in order to use a single task to handle all
 * CAN communication. This is done by using read() in non-blocking mode
 * and write in blocking mode.
 */
/* #define ONE_TASK */

/* Define this in order to only receive packets, can 
 * be usefull when debugging the receiver. This assumes
 * that the external board sends the messages by it own. 
 */
/* #define CANRX_ONLY */

/* Define this in order to only transmit packets, can 
 * be usefull when debugging the transmitter. No received
 * messages will be processed.
 */
/* #define CANTX_ONLY */

/* If RX_MESSAGES_CHANGED if defined it is assumed that the
 * received messages has been changed from the transmitted
 * message. In that case it is assumed that the ID field of
 * each message has been decremented once.
 *
 * This option is usefull when the messages are looped on an
 * external CAN board. CAN is not designed to receive the
 * exact same message as is beeing transmitted:
 *  WE SEND -> CAN_BUS -> External CAN Board changes the ID 
 *  field -> CAN_BUS -> WE RECEIVE and verify message
 */
/* #define RX_MESSAGES_CHANGED_ID
 * #define RX_MESSAGES_CHANGED_DATA
 */

/* Define this to get more statistics printed to console */
#undef PRINT_MORE_STATS

/* CAN Channel select */
int can_chan_sel = 0xA; /* Default to channel A */

#if defined(ONE_TASK) && defined(CANRX_ONLY)
 #error not possible to define both ONE_TASK and CANRX_ONLY
#endif

#if defined(CANRX_ONLY) && defined(CANTX_ONLY)
 #error not possible to define both CANRX_ONLY and CANTX_ONLY
#endif

/* Status printing Task entry point */
rtems_task status_task1(rtems_task_argument argument);

/* CAN routines */
int can_init(void);
void can_start(void);
void can_print_stats(void);

int status_init(void);
void status_start(void);

/* ========================================================= 
   initialisation */

rtems_task Init(
  rtems_task_argument ignored
)
{
  rtems_status_code status;

  printf("******** Initializing CAN test ********\n");

  /* Initialize Driver manager, in config.c */
	system_init();

  rtems_drvmgr_print_devs(0xfffff);
	rtems_drvmgr_print_topo();

  if ( can_init() ){
    printf("CAN INITIALIZATION FAILED, aborting\n");
    exit(1);
  }

  if ( status_init() ){
    printf("STATUS INITIALIZATION FAILED, aborting\n");
    exit(2);
  }

  can_start();

  status_start();

  status = rtems_task_delete(RTEMS_SELF);
}

rtems_id   tstatus;        /* array of task ids */
rtems_name tstatusname;     /* array of task names */

int status_init(void)
{
  rtems_status_code status;
  
  tstatusname = rtems_build_name( 'S', 'T', 'S', '0');
  
  /* Create a status printing task with the highest 
   * priority, this may result in CAN messages may be 
   * dropped. The CAN bus has no flow control stopping 
   * when receiver is full. ==> packets may be dropped
   * when CAN receive task doesn't get enough CPU. It
   * is helped by makeing sleep calls and increasing the
   * receive CAN buffer size.
   */
  status = rtems_task_create(
    tstatusname, 2, RTEMS_MINIMUM_STACK_SIZE * 4,
    RTEMS_DEFAULT_MODES | RTEMS_PREEMPT,
    RTEMS_DEFAULT_ATTRIBUTES, &tstatus
    );
  if ( status != RTEMS_SUCCESSFUL )
    return -1;
  
  return 0;
}

void status_start(void)
{
  rtems_status_code status;
  
  printf("Starting status task1\n");
  
  /* Starting Status task */
	status = rtems_task_start(tstatus, status_task1, 1);
}

/* Status Task */
rtems_task status_task1(
        rtems_task_argument unused
) 
{
  while(1){
    /* print stats */
    
    can_print_stats();
    sleep(2);

  }
}

/* CAN Implementation */

rtems_task can_task1(rtems_task_argument argument);
rtems_task can_task2(rtems_task_argument argument);

static rtems_id   tds[2];        /* array of task ids */
static rtems_name tnames[2];     /* array of task names */

/* File descriptors of /dev/grcan0 */
int canfd;

/* Print one CAN message to terminal */
void print_msg(int i, CANMsg *msg);

/* Initializes the 8 CAN messages in the global variable 
 * "CANMsg msgs[8]".
 */
void init_send_messages(void);

/* Verify content of CAN message 'msg' against msgs[index].
 * Returns what went wrong.
 */
int verify_msg(CANMsg *msg, int index);

/* ========================================================= 
   initialisation */

#define IOCTL(fd,num,arg) \
	{ \
  	if ( ioctl(fd,num,arg) != RTEMS_SUCCESSFUL ) { \
			printf("ioctl " #num " failed: errno: %d\n",errno); \
      return -1; \
		} \
  } 

int can_init(void)
{
  struct grcan_timing timing;
	struct grcan_selection selection;
  rtems_status_code status;
  int i;

  printf("******** Initializing GRCAN test ********\n");

  for ( i=0; i<2; i++){
    tnames[i] = rtems_build_name( 'T', 'D', 'C', '0'+i );
  }

  /*** Create, but do not start, CAN RX/TX tasks ***/
#if !defined(CANTX_ONLY)
  status = rtems_task_create(
    tnames[0], 1, RTEMS_MINIMUM_STACK_SIZE * 4,
    RTEMS_DEFAULT_MODES,
    RTEMS_DEFAULT_ATTRIBUTES, &tds[0]
    );
#endif

#if !defined(CANRX_ONLY) && !defined(ONE_TASK)
  status = rtems_task_create( 
    tnames[1], 3, RTEMS_MINIMUM_STACK_SIZE * 4,
    RTEMS_DEFAULT_MODES,
    RTEMS_DEFAULT_ATTRIBUTES, &tds[1]
    );
#endif

  /* Open GRCAN driver */
  canfd = open(GRCAN_DEVICE_NAME, O_RDWR);
  if ( canfd < 0 ){
    printf("Failed to open " GRCAN_DEVICE_NAME " driver (%d)\n", errno);
    return -1;
  }

  /* Start GRCAN driver */

  /* Set baud rate: 250k @ 30MHz */
#if 0
  timing.scaler = 3;
  timing.ps1 = 8;
  timing.ps2 = 5;
  timing.rsj = 1;
  timing.bpr = 1;
#endif
	
	/* Set baud rate: 250k @ 40MHz */
	timing.scaler = 7;
	timing.ps1 = 0xf;
	timing.ps2 = 0x3;
	timing.rsj = 0x1;
	timing.bpr = 0;
	
  /* Select CAN channel */
	if ( can_chan_sel == 0xa ){
		/* Channel A */
		selection.selection = 0;
		selection.enable0 = 0;
		selection.enable1 = 1;
	}else{
		/* Channel B */
		selection.selection = 1;
		selection.enable0 = 1;
		selection.enable1 = 0;
	}
		
  
  /* Set up CAN driver:
   *  ¤ baud rate
   *  ¤ Channel
   *  ¤ TX blocking, and wait for all data to be sent.
   *  ¤ RX non-blocking depending on ONE_TASK mode
   */
  IOCTL(canfd,GRCAN_IOC_SET_BTRS,&timing); /* set baudrate */
	IOCTL(canfd,GRCAN_IOC_SET_SELECTION,&selection); /* set baudrate */
  IOCTL(canfd,GRCAN_IOC_SET_TXCOMPLETE,1);
  IOCTL(canfd,GRCAN_IOC_SET_RXCOMPLETE,0);
#ifdef ONE_TASK
  /* in one task mode, we want TX to block instead */
  IOCTL(canfd,GRCAN_IOC_SET_RXBLOCK,0); 
#else
  /* in two task mode, we want TX _and_ RX to block */
  IOCTL(canfd,GRCAN_IOC_SET_RXBLOCK,1);
#endif
  IOCTL(canfd,GRCAN_IOC_SET_TXBLOCK,1);
  IOCTL(canfd,GRCAN_IOC_CLR_STATS,0);
  
  /* Start communication */
  IOCTL(canfd,GRCAN_IOC_START,0);

  return 0;
}

void can_start(void)
{
  rtems_status_code status;

  /* Starting receiver first */
#ifndef CANTX_ONLY
  status = rtems_task_start(tds[0], can_task1, 1);
#endif
#if !defined(CANRX_ONLY) && !defined(ONE_TASK)
  status = rtems_task_start(tds[1], can_task2, 1);
#endif
}


#define ID_GAISLER 0x2000

static CANMsg msgs[8];

void init_send_messages(void)
{ 
	/* Send 1 STD Message */
	msgs[0].extended = 0;
	msgs[0].rtr = 0;
	msgs[0].unused = 0;
	msgs[0].id = 10;
	msgs[0].len = 4;
	msgs[0].data[0] = 0x2;
	msgs[0].data[1] = 0xc4;
	msgs[0].data[2] = 0x4f;
	msgs[0].data[3] = 0xf2;
	msgs[0].data[4] = 0x23;
		
	/* Send 3 EXT Message */
	msgs[1].extended = 1;
	msgs[1].rtr = 0;
	msgs[1].unused = 0;
	msgs[1].id = 10;
	msgs[1].len = 4+1;
	msgs[1].data[0] = 0x2;
	msgs[1].data[1] = 0xc4;
	msgs[1].data[2] = 0x4f;
	msgs[1].data[3] = 0xf2;
	msgs[1].data[4] = 0x23;
  msgs[1].data[5] = 0xa2;
	
	msgs[2].extended = 1;
	msgs[2].rtr = 0;
	msgs[2].unused = 0;
	msgs[2].id = 10+880;
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
	msgs[3].unused = 0;
	msgs[3].id = 0xff | ID_GAISLER;
	msgs[3].len = 7;
	msgs[3].data[0] = 'G';
	msgs[3].data[1] = 'a';
	msgs[3].data[2] = 'i';
	msgs[3].data[3] = 's';
	msgs[3].data[4] = 'l';
	msgs[3].data[5] = 'e';
	msgs[3].data[6] = 'r';
  
  	/* Send 1 STD Message */
	msgs[4].extended = 0;
	msgs[4].rtr = 0;
	msgs[4].unused = 0;
	msgs[4].id = 10;
	msgs[4].len = 4;
	msgs[4].data[0] = 0x2;
	msgs[4].data[1] = 0xc4;
	msgs[4].data[2] = 0x4f;
	msgs[4].data[3] = 0xf2;
	msgs[4].data[4] = 0x23;
		
	/* Send 3 EXT Message */
	msgs[5].extended = 1;
	msgs[5].rtr = 0;
	msgs[5].unused = 0;
	msgs[5].id = 10;
	msgs[5].len = 4+1;
	msgs[5].data[0] = 0x2;
	msgs[5].data[1] = 0xc4;
	msgs[5].data[2] = 0x4f;
	msgs[5].data[3] = 0xf2;
	msgs[5].data[4] = 0x23;
  msgs[5].data[5] = 0xa2;
	
	msgs[6].extended = 1;
	msgs[6].rtr = 0;
	msgs[6].unused = 0;
	msgs[6].id = 10+880;
	msgs[6].len = 8;
	msgs[6].data[0] = 0xaa;
	msgs[6].data[1] = 0xbb;
	msgs[6].data[2] = 0x11;
	msgs[6].data[3] = 0x22;
	msgs[6].data[4] = 'U';
	msgs[6].data[5] = 0x12;
	msgs[6].data[6] = 0xff;
	msgs[6].data[7] = 0x00;

	msgs[7].extended = 1;
	msgs[7].rtr = 0;
	msgs[7].unused = 0;
	msgs[7].id = 0xff | ID_GAISLER;
	msgs[7].len = 7;
	msgs[7].data[0] = 'G';
	msgs[7].data[1] = 'a';
	msgs[7].data[2] = 'i';
	msgs[7].data[3] = 's';
	msgs[7].data[4] = 'l';
	msgs[7].data[5] = 'e';
	msgs[7].data[6] = 'r';
}

/* Verify content of a CAN message */
int verify_msg(CANMsg *msg, int index)
{
  int i;
  CANMsg *src = &msgs[index];
  
  if ( (msg->extended && !src->extended) || (!msg->extended && src->extended) ){
    printf("Expected %d but got %d\n",src->extended,msg->extended);
		return -1;
  }

  if ( msg->rtr != src->rtr ){
    return -2;
  }  
  
#ifdef RX_MESSAGES_CHANGED_ID
  /* Decremented the ID once */
  if ( msg->id != (src->id-1) ){
#else
  if ( msg->id != src->id ){
#endif
    printf("Expected 0x%x but got 0x%x\n",src->id,msg->id);
    return -3;
  }
  
  if ( msg->len != src->len ){
    return -4;
  }
  for(i=0; i<msg->len; i++){
#ifdef RX_MESSAGES_CHANGED_DATA
    if ( msg->data[i] != (src->data[i]+1) )
#else
    if ( msg->data[i] != src->data[i] ) 
#endif
      return -5-i;
  }
  return 0;
}

/* Staticstics */
static volatile int rxpkts=0, txpkts=0;
static volatile int rx_syncs=0,rx_errors=0;

/* RX Task */
rtems_task can_task1(
        rtems_task_argument unused
) 
{
  CANMsg rxmsgs[10];
  int i,j,cnt,index=0,error,e;

  int wcnt,ofs;
  int last;
	
#if defined(CANRX_ONLY) || defined(ONE_TASK)
	printf("Initing messages\n");
  init_send_messages();
#endif

#ifdef ONE_TASK
	printf("************** MESSAGES THAT WILL BE TRANSMITTED *************\n");
	print_msg(1,&msgs[0]);
	print_msg(2,&msgs[1]);
	print_msg(3,&msgs[2]);
	print_msg(4,&msgs[3]);
	printf("**************************************************************\n");

	printf("******************* Start of transmission ********************\n");
#endif
	
#ifdef ONE_TASK
  txpkts=0;
#endif

  last=0;  
  ofs=0;
  wcnt=0;

  while(1){
    if ( (cnt=read(canfd,rxmsgs,10*sizeof(CANMsg))) < 1 ){

#ifdef ONE_TASK      
      /* In non-blocking RX mode... 
       * Send messages while instead of blocking read waiting
       * for new messages to arrive.
       *
       * TX is blocking and waiting to complete.
       */
      wcnt=write(canfd,&msgs[ofs],4*sizeof(CANMsg));
      if ( wcnt > 0 ){
        txpkts += wcnt/sizeof(CANMsg);      
        ofs+=wcnt/sizeof(CANMsg);
        if ( ofs > 3 )  
          ofs-=4;
      }else
        sched_yield();

#if 0
      /* Wait a bit after each 256 messages */
      if ( (txpkts & 0x100) && !(last & 0x100) )
        rtems_task_wake_after(4);
      last = txpkts;
#endif
      
      continue;
      
#else
      /* blocking mode: should not fail unless CAN errors.
       * In this simple example we don't handle CAN errors
       */
      printf("CAN read() failed: %d\n",errno);
      break;
#endif
    }
    
    /* Statistics */
    rxpkts+=cnt/sizeof(CANMsg);
    
    
/*    printf("Got %d messages\n",cnt/sizeof(CANMsg));*/

    /* For every message received we compare the content against 
     * expected content.
     *
     * If a message have been dropped we synchronize with the
     * message stream to avoid getting multiple errors from one
     * dropped message.
     *
     */
    for(i=0; i<(cnt/sizeof(CANMsg)); i++){
      error = verify_msg(&rxmsgs[i],index);
      if ( error ){
        printf("Message rx error: %d, index: %d\n",error,index);
        
        /* Print message */
        print_msg(0,&rxmsgs[i]);

        /* Try to sync if one has been lost */
        e=0;
        for(j=0; j<4; j++){
          if ( !verify_msg(&rxmsgs[i],j) ){
            printf("Synced from message %d to %d\n",index,j);
            rx_syncs++;
            index = j;
            e=1;
            break;
          }
        }
        if ( e!=1 )
          rx_errors++;
      }
      index++;
      if ( index > 3 )
        index = 0;
    }
  }
  
	while(1) {
		printf("Sleeping Task1\n");
		sleep(1);
	}
}



/* TX Task */
rtems_task can_task2(
        rtems_task_argument unused
) 
{  
  int cnt,ofs;
  int last;
  
	/* Print messages that we be sent to console */
	printf("************** MESSAGES THAT WILL BE TRANSMITTED *************\n");
  init_send_messages();  
	print_msg(1,&msgs[0]);
	print_msg(2,&msgs[1]);
	print_msg(3,&msgs[2]);
	print_msg(4,&msgs[3]);
	printf("**************************************************************\n");

	printf("******************* Start of transmission ********************\n");
	
  last=0;
  txpkts=0;
  ofs=0;
  while(1){

  	/* Blocking transmit request. Returns when all messages
     * requested has been scheduled for transmission (not actually
     * sent, but taken care of by driver).
     */
    cnt=write(canfd,&msgs[ofs],4*sizeof(CANMsg));
    if ( cnt > 0 ){
      /* Increment statistics */
      txpkts += cnt/sizeof(CANMsg);      
      ofs+=cnt/sizeof(CANMsg);
      if ( ofs > 3 )  
        ofs-=4;
    }else{
      sched_yield();
      printf("TX CAN TASK: write failed: %d (%s)\n",errno,strerror(errno));
    }

#if 0
    /* Wait a bit after each 256 messages */
    if ( (txpkts & 0x100) && !(last & 0x100) )
      rtems_task_wake_after(4);
    last = txpkts;
#endif
   
  }
	  
	while(1) {
		printf("Sleeping Task 2\n");
		sleep(1);
	}
}


/* CAN HELP DEBUG FUNCTIONS */
char *msgstr_type[2] = {"STD", "EXT"};
char *msgstr_rtr[2] = {"", " RTR"};

/* PRINT A CAN MESSAGE FROM DATA STRUCTURE */
void print_msg(int i, CANMsg *msg){
	int j;
	char data_str_buf[64];
	int ofs;
	
	if ( !msg )
		return;
	
	if ( i > 0 ){
		printf("MSG[%d]: %s%s length: %d, id: 0x%x\n",i,msgstr_type[(int)msg->extended],msgstr_rtr[(int)msg->rtr],msg->len,msg->id);
		/* print data */
		if ( msg->len > 0 ){
			ofs = sprintf(data_str_buf,"MSGDATA[%d]: ",i);
			for(j=0; j<msg->len; j++){
				ofs+=sprintf(data_str_buf+ofs,"0x%02x ",msg->data[j]);
			}
			printf("%s  ",data_str_buf);
			ofs=0;
			for(j=0; j<msg->len; j++){
				if ( isalnum(msg->data[j]) )
					ofs+=sprintf(data_str_buf+ofs,"%c",msg->data[j]);
				else
					ofs+=sprintf(data_str_buf+ofs,".");
			}
			printf("%s\n",data_str_buf);
		}
	}else{
		printf("MSG: %s%s length: %d, id: 0x%x\n",msgstr_type[(int)msg->extended],msgstr_rtr[(int)msg->rtr],msg->len,msg->id);
		/* print data */
		if ( msg->len > 0 ){
			ofs = sprintf(data_str_buf,"MSGDATA: ");
			for(j=0; j<msg->len; j++){
				ofs+=sprintf(data_str_buf+ofs,"0x%02x ",msg->data[j]);
			}
			printf("%s  ",data_str_buf);
			ofs=0;
			for(j=0; j<msg->len; j++){
				if ( isalnum(msg->data[j]) )
					ofs+=sprintf(data_str_buf+ofs,"%c",msg->data[j]);
				else
					ofs+=sprintf(data_str_buf+ofs,".");
			}
			printf("%s\n",data_str_buf);
		}
	}
}

/* Print statistics gathered by RX and TX tasks, also
 * print statistics from driver.
 */
void can_print_stats(void)
{
  struct grcan_stats stats;
  static int cnt=0;
  
  /* Get stats from GRCAn driver to print */
  if ( ioctl(canfd,GRCAN_IOC_GET_STATS,&stats) == 0 ) {
    /* Got stats from driver */
    
#ifdef PRINT_MORE_STATS
    /* Print extra stats */
    printf("CAN PASSV:   %d\n",stats.passive_cnt);
    rtems_task_wake_after(4);
    printf("CAN OVERRUN: %d\n",stats.overrun_cnt);
    rtems_task_wake_after(4);
    printf("CAN TXLOSS:  %d\n",stats.txloss_cnt);
    rtems_task_wake_after(4);
#endif
    if ( stats.ahberr_cnt )
      printf("CAN AHB:     %d\n",stats.ahberr_cnt);
    printf("CAN INTS:     %d\n",stats.ints);
    rtems_task_wake_after(4);
  }
  printf("CAN RXPKTS:   %d\n",rxpkts);
  rtems_task_wake_after(4);
  
  /* Print only number of RX syncs every tenth time */
  if ( cnt++ >= 10 ){
    cnt=0;
    printf("CAN RXSYNCS:  %d\n",rx_syncs);
    if ( rx_errors > 0)
      printf("CAN RXERRORS: %d\n",rx_errors);
  }
  
  printf("CAN TXPKTS:   %d\n",txpkts);
  rtems_task_wake_after(4);
}
