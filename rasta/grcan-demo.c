/* Simple GRCAN interface test.
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
#include <sched.h>
#include <ctype.h>
#include <bsp.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>

#include <grcan.h>

#undef CANRX_ONLY

rtems_task can_task1(rtems_task_argument argument);
rtems_task can_task2(rtems_task_argument argument);

static rtems_id   tds[2];        /* array of task ids */
static rtems_name tnames[2];     /* array of task names */

int canfd;
char tmpbuf[256];

void print_msg(int i, CANMsg *msg);
void init_send_messages(void);
void inloop_init(void);

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
  
  printf("******** Initiating RASTA CAN test ********\n");

  for ( i=0; i<2; i++){
    tnames[i] = rtems_build_name( 'T', 'D', 'C', '0'+i );
  }  

  inloop_init();

  status = rtems_task_create(
    tnames[0], 1, RTEMS_MINIMUM_STACK_SIZE * 4,
    RTEMS_DEFAULT_MODES | RTEMS_TIMESLICE,
    RTEMS_DEFAULT_ATTRIBUTES, &tds[0]
    );

  status = rtems_task_create( 
    tnames[1], 1, RTEMS_MINIMUM_STACK_SIZE * 4,
    RTEMS_DEFAULT_MODES| RTEMS_TIMESLICE,
    RTEMS_DEFAULT_ATTRIBUTES, &tds[1]
    );
  
  /* Open GRCAN driver */
  canfd = open("/dev/rastaio0/grcan0",O_RDWR);
  if ( canfd < 0 ){
    printf("Failed to open /dev/rastaio0/grcan0 driver (%d)\n", errno);
    return -1;
  }
				
  /* Start GRCAN driver */

  /* 250k @ 30MHz */
  timing.scaler = 3;
  timing.ps1 = 8;
  timing.ps2 = 5;
  timing.rsj = 1;
  timing.bpr = 1;
	
	selection.selection = 0;
	selection.enable0 = 0;
	selection.enable1 = 1;
  
  IOCTL(canfd,GRCAN_IOC_SET_BTRS,&timing); /* set baudrate */
	IOCTL(canfd,GRCAN_IOC_SET_SELECTION,&selection); /* set baudrate */
  IOCTL(canfd,GRCAN_IOC_SET_TXCOMPLETE,1);
  IOCTL(canfd,GRCAN_IOC_SET_RXCOMPLETE,0);
  IOCTL(canfd,GRCAN_IOC_SET_RXBLOCK,1);
  IOCTL(canfd,GRCAN_IOC_SET_TXBLOCK,1);
  IOCTL(canfd,GRCAN_IOC_CLR_STATS,0);
  IOCTL(canfd,GRCAN_IOC_START,0);
				
  return 0;
}

void can_start(void)
{
  rtems_status_code status;

  /* Starting receiver first */
  status = rtems_task_start(tds[0], can_task1, 1);
#ifndef CANRX_ONLY  
  status = rtems_task_start(tds[1], can_task2, 1);
#endif

}


#define ID_GAISLER 0x2000

#define TEST_CHECK
#define TEST_PRINT
/*#define TEST_BIG_CHUNK*/

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

int verify_msg(CANMsg *msg, int index)
{
  int i;
  CANMsg *src = &msgs[index];
  
  if ( msg->extended != src->extended ){
    return -1;
  }

  if ( msg->rtr != src->rtr ){
    return -2;
  }  
  
  /* inverted bit 0 in ID */
  if ( msg->id != (src->id-1) ){
    /*printf("Expected 0x%x but got 0x%x\n",src->id-1,msg->id);*/
    return -3;
  }
  
  if ( msg->len != src->len ){
    return -4;
  }
  for(i=0; i<msg->len; i++){
    if ( msg->data[i] != src->data[i] ) 
      return -5-i;
  }
  return 0;
}


volatile int inloop=0;
#define WAIT_FOR_INLOOP_CNT 100
rtems_id inloop_sem;

void inloop_init(void){
  rtems_semaphore_create(rtems_build_name('i', 'n', 'l', 'o'),
        0,
        RTEMS_FIFO|RTEMS_SIMPLE_BINARY_SEMAPHORE|RTEMS_NO_INHERIT_PRIORITY|\
        RTEMS_LOCAL|RTEMS_NO_PRIORITY_CEILING, 
        0,
        &inloop_sem);
  inloop=0;
}

int get_inloop(void){
  return inloop;
}

void increment_inloop(int cnt){
  inloop+=cnt;
}

void decrement_inloop(int cnt){
  int tmp = inloop;
  inloop=tmp-cnt;
/*   
  if ( tmp > (WAIT_FOR_INLOOP_CNT) ){
    rtems_semaphore_release(inloop_sem);
  }
  */
}

void wait_for_inloop(void){

  while( get_inloop() > WAIT_FOR_INLOOP_CNT ){
    sched_yield();
  }
  
/*
  while ( get_inloop() > WAIT_FOR_INLOOP_CNT ){
    rtems_semaphore_obtain(inloop_sem,RTEMS_WAIT,RTEMS_NO_TIMEOUT);
  }
  */
}

volatile int rxpkts=0,txpkts=0;
static volatile int rx_syncs=0,rx_errors=0;

/* RX Task */
rtems_task can_task1(
        rtems_task_argument unused
) 
{
  CANMsg rxmsgs[10];
  int cnt;
#ifdef RX_VERIFY
  int i,j,index=0,error,e;
#endif

#ifdef RX_ONLY  
  init_send_messages();
#endif

  while(1){
    if ( (cnt=read(canfd,rxmsgs,10*sizeof(CANMsg))) < 1 ){
      break;
    } 
    
    rxpkts+=cnt/sizeof(CANMsg);
#ifdef RX_VERIFY
/*    printf("Got %d messages\n",cnt/sizeof(CANMsg));*/
    for(i=0; i<(cnt/sizeof(CANMsg)); i++){
      error = verify_msg(&rxmsgs[i],index);
      if ( error ){
        /*printf("Message rx error: %d\n",error);
        print_msg(0,&rxmsgs[i]);*/
        /* Try to sync if one has been lost */
        e=0;
        for(j=0; j<4; j++){
          if ( !verify_msg(&rxmsgs[i],j) ){
            /*printf("Synced from message %d to %d\n",index,j);*/
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
#endif
    
   /* decrement_inloop(cnt/sizeof(CANMsg));*/
    
/*    rtems_task_wake_after(1);*/
    sched_yield();
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
  
	
	printf("************** MESSAGES THAT WILL BE TRANSMITTED *************\n");
  init_send_messages();  
	print_msg(1,&msgs[0]);
	print_msg(2,&msgs[1]);
	print_msg(3,&msgs[2]);
	print_msg(4,&msgs[3]);
	printf("**************************************************************\n");

	printf("******************* Start of transmission ********************\n");
	
  txpkts=0;
  ofs=0;
  while(1){
    /* Not more than 114 messages may be out 
     * since we only have a receivfe buffer of 124 messages 
     */
     /*
    if ( get_inloop() > WAIT_FOR_INLOOP_CNT ){
      sched_yield();
      continue;
    }
    */
/*    wait_for_inloop();*/
/*    
    if ( (cnt=write(canfd,msgs,4*sizeof(CANMsg))) != (4*sizeof(CANMsg)) ){
      break;
    }
*/
    cnt=write(canfd,&msgs[ofs],4*sizeof(CANMsg));
    if ( cnt > 0 ){
      txpkts += cnt/sizeof(CANMsg);      
      ofs+=cnt/sizeof(CANMsg);
      if ( ofs > 3 )  
        ofs-=4;
    }else
      sched_yield();


/*    txpkts+= 4;*/
    
/*    increment_inloop(4);*/
/*    printf("Sent %d messages\n",cnt/sizeof(CANMsg));*/

/*    rtems_task_wake_after(1);*/
    sched_yield();
  }
	  
	while(1) {
		printf("Sleeping Task 2\n");
		sleep(1);
	}
}


/* CAN HELP DEBUG FUNCTIONS */
char *msgstr_type[2] = {"STD", "EXT"};
char *msgstr_rtr[2] = {"", " RTR"};

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

static unsigned int __inline__ _tmp_read_nocache(unsigned int address)
{
  unsigned int tmp;
  asm(" lda [%1]1, %0 "
    : "=r"(tmp)
    : "r"(address)
  );
  return tmp;
}

#define READ_REG(adr) _tmp_read_nocache((unsigned int)(adr))
void can_print_stats(void)
{
  struct grcan_stats stats;
  volatile unsigned int tmp,*p = (unsigned int *)0x80080200;
  int dont=0;
  static int cnt=0;

  if ( READ_REG(p) != 2 ){
    dont=1;
  }
  
  /* No stats to print */
  if ( ioctl(canfd,GRCAN_IOC_GET_STATS,&stats) == 0 ) {
/*        printf("CAN PASSV:   %d\n",stats.passive_cnt);
        rtems_task_wake_after(4);
        printf("CAN OVERRUN: %d\n",stats.overrun_cnt);
        rtems_task_wake_after(4);
        printf("CAN TXLOSS:  %d\n",stats.txloss_cnt);
        rtems_task_wake_after(4);
        */
        if ( stats.ahberr_cnt )
          printf("CAN AHB:     %d\n",stats.ahberr_cnt);
        printf("CAN INTS:     %d\n",stats.ints);
        rtems_task_wake_after(4);
  }
  printf("CAN RXPKTS:   %d\n",rxpkts);
  rtems_task_wake_after(4);
  
  if ( cnt++ >= 10 ){
    cnt=0;
    printf("CAN RXSYNCS:  %d\n",rx_syncs);
    if ( rx_errors > 0)
      printf("CAN RXERRORS: %d\n",rx_errors);
  }
  
  if ( !dont && (READ_REG(p) != 2) ){
    dont=1;
  }
  
  printf("CAN TXPKTS:   %d\n",txpkts);
  rtems_task_wake_after(4);
  tmp=READ_REG(p);
  /*printf("CAN  %d, %d\n",dont,tmp);*/
  /* *p = 1; */
  if ( !dont && (tmp == 2) ){
    *p = 1;
    printf("CAN: reenabled\n");
  }
}
