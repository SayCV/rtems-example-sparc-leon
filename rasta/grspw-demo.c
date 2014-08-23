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
#include <bsp.h> /* for device driver prototypes */
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sched.h>
#include <string.h>

#include <ambapp.h>
#include <grspw.h>

#define NODE_ADR_RX 10
#define NODE_ADR_TX 22

rtems_task spw_task1(rtems_task_argument argument);
rtems_task spw_task2(rtems_task_argument argument);

static rtems_id   tds[2];         /* array of task ids */
static rtems_name tnames[2];       /* array of task names */

static int fd0,fd1;

/* ========================================================= 
   initialisation */



int spw_init(void)
{ 
  rtems_status_code status;
  int i;
  
  printf("******** Initiating RASTA SPW test ********\n");

  for ( i=0; i<2; i++){
    tnames[i] = rtems_build_name( 'T', 'D', 'S', '0'+i );
  }  
      
  status = rtems_task_create(
    tnames[0], 1, RTEMS_MINIMUM_STACK_SIZE * 4,
    RTEMS_DEFAULT_MODES | RTEMS_TIMESLICE,
    RTEMS_DEFAULT_ATTRIBUTES, &tds[0]
    );
  if ( status != RTEMS_SUCCESSFUL ){
    printf("Failed to create spw task 1 (%d)\n",status);
    return -1;
  }
  
  status = rtems_task_create( 
    tnames[1], 1, RTEMS_MINIMUM_STACK_SIZE * 4,
    RTEMS_DEFAULT_MODES| RTEMS_TIMESLICE,
    RTEMS_DEFAULT_ATTRIBUTES, &tds[1]
    );
  if ( status != RTEMS_SUCCESSFUL ){
    printf("Failed to create spw task 2 (%d)\n",status);
    return -1;
  }
  
  fd0 = open("/dev/rastaio0/grspw0",O_RDWR);
  if ( fd0 < 0 ){
    printf("Failed to open /dev/rastaio0/grspw0 driver (%d)\n", errno);
    return -1;
  }
  
  fd1 = open("/dev/rastaio0/grspw1",O_RDWR);
  if ( fd1 < 0 ){
    printf("Failed to open /dev/rastaio0/grspw1 driver (%d)\n", errno);
    return -1;
  }  
				
	/* Start GRSPW driver */  
#define IOCTL(fd,num,arg) \
	{ \
    int ret; \
  	if ( (ret=ioctl(fd,num,arg)) != RTEMS_SUCCESSFUL ) { \
			printf("ioctl " #num " failed: errno: %d (%d,%d)\n",errno,ret,RTEMS_SUCCESSFUL); \
      return -1;\
		} \
  } 
  
  printf("Starting spacewire links\n");

  IOCTL(fd0,SPACEWIRE_IOCTRL_SET_COREFREQ,30000); /* make driver calculate timings from 30MHz spacewire clock */
  IOCTL(fd0,SPACEWIRE_IOCTRL_SET_NODEADDR,NODE_ADR_TX);
  IOCTL(fd0,SPACEWIRE_IOCTRL_SET_RXBLOCK,1);
  IOCTL(fd0,SPACEWIRE_IOCTRL_SET_TXBLOCK,0);
  IOCTL(fd0,SPACEWIRE_IOCTRL_SET_TXBLOCK_ON_FULL,1);
  IOCTL(fd0,SPACEWIRE_IOCTRL_SET_RM_PROT_ID,1); /* remove protocol id */
/*  IOCTL(fd0,SPACEWIRE_IOCTRL_SET_CLKDIV,0);*/

  IOCTL(fd1,SPACEWIRE_IOCTRL_SET_COREFREQ,30000); /* make driver calculate timings from 30MHz spacewire clock */
  IOCTL(fd1,SPACEWIRE_IOCTRL_SET_NODEADDR,NODE_ADR_RX);
  IOCTL(fd1,SPACEWIRE_IOCTRL_SET_RXBLOCK,1);
  IOCTL(fd1,SPACEWIRE_IOCTRL_SET_TXBLOCK,0);
  IOCTL(fd1,SPACEWIRE_IOCTRL_SET_TXBLOCK_ON_FULL,1);
  IOCTL(fd1,SPACEWIRE_IOCTRL_SET_RM_PROT_ID,1); /* remove protocol id */
/*  IOCTL(fd1,SPACEWIRE_IOCTRL_SET_CLKDIV,0);*/


  IOCTL(fd0,SPACEWIRE_IOCTRL_START,2000); /* change timeout to 2000ticks */

  IOCTL(fd1,SPACEWIRE_IOCTRL_START,2000); /* change timeout to 2000ticks */
  
  return 0;
}

void spw_start(void)
{
  rtems_status_code status;
  
  printf("******** Starting RASTA SPW test ********\n");
  
  /* Starting receiver first */
  status = rtems_task_start(tds[1], spw_task2, 1);
  status = rtems_task_start(tds[0], spw_task1, 1);
}


#define TEST_CHECK
#define TEST_PRINT
/*#define TEST_BIG_CHUNK*/
#define RXPKT_BUF 5
#define PKTSIZE 1000
struct packet_hdr {
	unsigned char addr;
	unsigned char protid;
	unsigned char dummy;
  unsigned char channel;
  unsigned char data[PKTSIZE];
};

int blocked=0;
int isBlocked(void)
{
  return blocked;
}

void Block(void)
{
  blocked = 0;
}

void unBlock(void)
{
  blocked = 0;
}

int rx_bytes=0;
int rx_pkts=0;
int rx_errors=0;

int tx_bytes=0;
int tx_pkts=0;

/* RX Task */
static unsigned char rxpkt[PKTSIZE*RXPKT_BUF];
static struct packet_hdr txpkts[1];

void init_pkt(struct packet_hdr *p){
  int i;
  unsigned char j=0;
  
  p->addr = NODE_ADR_RX;
  p->protid = 50;
  p->dummy = 0x01;
  p->channel = 0x01;
  for(i=0; i<PKTSIZE; i++){
    p->data[i] = j;
    j++;
  }
}

rtems_task spw_task1(
        rtems_task_argument unused
) 
{ 
  
  unsigned char i;
  int j;
  int len,cnt=0;
  int loop;
  printf("SpaceWire TX Task started\n");
  
  for(i=0; i<1; i++)
    init_pkt(&txpkts[i]);
    
  i=0;
  loop=0;
  while ( 1 ) {
    for(j=0;j<PKTSIZE;j++){
      txpkts[0].data[j] = i++;
    }

    if ( (len=write(fd0,txpkts,PKTSIZE+4)) < 0 ){
      printf("Failed to write errno:%d (%d)\n",errno,cnt);
      break;
    }
    tx_bytes+=len;
    tx_pkts++;
/*    rtems_task_wake_after(1);*/
    sched_yield();
    cnt++;
  }

	while(1) {
		printf("SPW Task1: Sleeping\n");
		sleep(1);
	}
}

/* TX Task */
rtems_task spw_task2(
        rtems_task_argument unused
) 
{
  int len;
  int cnt=0;
  int j;
  unsigned char i=0;
  unsigned int tot=0;
  int pktofs=0;

  printf("SpaceWire RX Task started\n");
  #if 1
  while(1){
    /*memset(&rxpkt,0,sizeof(rxpkt));*/
    
    if ( (len=read(fd1,&rxpkt[0],PKTSIZE*RXPKT_BUF)) < 1 ){
      printf("Failed read: len: %d, errno: %d (%d)\n",len,errno,cnt);
      break;
    }
    
    /* skip first 2bytes (vchan and dummy) */
    if ( (rxpkt[0]==1) && (rxpkt[1]==1) ){
      j=2; /* strip virtual channel protocol, non-ssspw device */
    }else{
      j=0; /* hardware uses virtual channel protocol, hw already stripped it */
    }
    
    for(; j<len; j++){
      
      if ( (rxpkt[j] != i) ){
        printf("Data differ at %d, expected 0x%x got 0x%x (%d)\n",tot,i,rxpkt[j],pktofs);
        i=rxpkt[j];
        rx_errors++;
      }
      i++;
      tot++;
      pktofs++;
      if ( pktofs >= PKTSIZE ){
        pktofs=0;
      }
    }
    
    rx_bytes+=len;
    rx_pkts++;
    sched_yield();
    cnt++;
  }
  #endif
	while(1) {
		printf("SPW Task2: Sleeping\n");
		sleep(1);
	}
}

void spw_print_stats(void){
  printf("SPW RX: bytes: %d, times: %d, data errors: %d\n",rx_bytes,rx_pkts,rx_errors);
/*  rtems_task_wake_after(4);*/
    sched_yield();
  printf("SPW TX: bytes: %d, packets: %d\n",tx_bytes,tx_pkts);
/*  rtems_task_wake_after(4);*/
    sched_yield();
}
