/* Simple APBUART interface test.
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
 
#include <apbuart.h>

rtems_task uart_task(rtems_task_argument uart);

static rtems_id   tds[2];        /* array of task ids */
static rtems_name tnames[2];     /* array of task names */

int uartfd0,uartfd1;

struct uart_priv {
  int fd;
  int minor;
  unsigned int rxtot;
  unsigned int txtot;
};

struct uart_priv u0,u1;

/* ========================================================= 
   initialisation */

#define IOCTL(fd,num,arg) \
	{ \
    int ret; \
  	if ( (ret=ioctl(fd,num,arg)) != RTEMS_SUCCESSFUL ) { \
			printf("ioctl " #num " failed: errno: %d (%d)\n",errno,ret); \
      return -1;\
		} \
  }


int uart_init(void)
{
  rtems_status_code status;
  int i;
  
  printf("******** Initiating RASTA UART test ********\n");
  
  for ( i=0; i<2; i++){
    tnames[i] = rtems_build_name( 'T', 'D', 'U', '0'+i );
  }

  status = rtems_task_create(
              tnames[0], 1, RTEMS_MINIMUM_STACK_SIZE * 2,
              RTEMS_TIMESLICE,
              RTEMS_DEFAULT_ATTRIBUTES, &tds[0]
              );
  
  status = rtems_task_create( 
              tnames[1], 1, RTEMS_MINIMUM_STACK_SIZE * 2,
              RTEMS_TIMESLICE,
              RTEMS_DEFAULT_ATTRIBUTES, &tds[1]
              );
  
  /* Open UART0 driver */
  u0.minor = 0;  
  u0.rxtot = 0;
  u0.txtot = 0;
  u0.fd = open("/dev/rastaio0/apbuart0",O_RDWR);
  if ( u0.fd < 0 ){
    printf("Failed to open /dev/rastaio0/apbuart0 driver (%d)\n", errno);
    return -1;
  }
  
  /* Open UART1 driver */
  u1.minor = 1;
  u1.rxtot = 0;
  u1.txtot = 0;
  u1.fd = open("/dev/rastaio0/apbuart1",O_RDWR);
  if ( u1.fd < 0 ){
    printf("Failed to open /dev/rastaio0/apbuart1 driver (%d)\n", errno);
    return -1;
  }
  				
  /* Start UART0 driver */
  printf("Initiating APBUART0 device driver\n");
	IOCTL(u0.fd, APBUART_SET_BAUDRATE, 38400); /* stream mode */
	IOCTL(u0.fd, APBUART_SET_BLOCKING, APBUART_BLK_RX | APBUART_BLK_TX);
	IOCTL(u0.fd, APBUART_SET_TXFIFO_LEN, 64);  /* Transmitt buffer 64 chars */
	IOCTL(u0.fd, APBUART_SET_RXFIFO_LEN, 256); /* Receive buffer 256 chars */
	IOCTL(u0.fd, APBUART_SET_ASCII_MODE, 1); /* Make \n go \n\r or \r\n */
	IOCTL(u0.fd, APBUART_CLR_STATS, 0);
	IOCTL(u0.fd, APBUART_START, 0);

  /* Start UART1 driver */
  printf("Initiating APBUART1 device driver\n");
	IOCTL(u1.fd, APBUART_SET_BAUDRATE, 38400); /* stream mode */
	IOCTL(u1.fd, APBUART_SET_BLOCKING, APBUART_BLK_RX | APBUART_BLK_TX);
	IOCTL(u1.fd, APBUART_SET_TXFIFO_LEN, 64);  /* Transmitt buffer 64 chars */
	IOCTL(u1.fd, APBUART_SET_RXFIFO_LEN, 256); /* Receive buffer 256 chars */
	IOCTL(u1.fd, APBUART_SET_ASCII_MODE, 1); /* Make \n go \n\r or \r\n */
	IOCTL(u1.fd, APBUART_CLR_STATS, 0);
	IOCTL(u1.fd, APBUART_START, 0);
  
  return 0;
}

void uart_start(void)
{
  rtems_status_code status;
  
  printf("******** Starting RASTA UART test ********\n");
  
  /* Starting receiver first */
	status = rtems_task_start(tds[0], uart_task, (rtems_task_argument)&u0);
	status = rtems_task_start(tds[1], uart_task, (rtems_task_argument)&u1);
  
}

#define TEST_CHECK
/*#define TEST_PRINT*/
/*#define TEST_BIG_CHUNK*/

/* RX Task */
rtems_task uart_task(
        rtems_task_argument uart
) 
{
  struct uart_priv *u = (struct uart_priv *)uart;
  int rlen,wlen,left,err=0;
  char buf[256];
  
  if ( !u ){
    printf("Argument to UART task is wrong (%d)\n",uart);
    exit(1);
  }
  
	printf("UART%d task started\n",u->minor);
  
  while(!err){
    rlen = read(u->fd,buf,256);
    if ( rlen < 0 ){
      err=1;
      break;
    }
    
    /* increment status */
    u->rxtot += rlen;
    
    /* Echo chars */
    left = rlen;
    while(left>0) {
      wlen = write(u->fd,&buf[rlen-left],left);
      if ( wlen < 1 ) {
        err=2;
        break;
      }
      left -= wlen;
      /* increment status */
      u->txtot += wlen;
    }
  }
  
  printf("UART%d task failed: err: %d, errno: %d\n",u->minor,err,errno);
}

void uart_print_stats(void){
  printf("UART0 RX: %d\n",u0.rxtot);
  rtems_task_wake_after(4);
  printf("UART0 TX: %d\n",u0.txtot);
  rtems_task_wake_after(4);
  printf("UART1 RX: %d\n",u1.rxtot);
  rtems_task_wake_after(4);
  printf("UART1 TX: %d\n",u1.txtot);
  rtems_task_wake_after(4);
}
