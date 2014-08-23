/* Simple SpaceWire interface test for 2 boards.
 * When using two boards define MULTI_BOARD and 
 * one of TASK_RX or TASK_TX.
 *
 * The main SpaceWire example for oe board is rtems-spacewire.
 *
 * Gaisler Research 2007,
 * Daniel Hellström
 *
 */

#define CONFIGURE_INIT
#include <bsp.h> /* for device driver prototypes */
rtems_task Init( rtems_task_argument argument);	/* forward declaration needed */

/* configuration information */
#define CONFIGURE_APPLICATION_NEEDS_CONSOLE_DRIVER
#define CONFIGURE_APPLICATION_NEEDS_NULL_DRIVER 1
#define CONFIGURE_APPLICATION_NEEDS_CLOCK_DRIVER

#define CONFIGURE_MAXIMUM_TASKS             8
#define CONFIGURE_RTEMS_INIT_TASKS_TABLE
#define CONFIGURE_EXTRA_TASK_STACKS         (3 * RTEMS_MINIMUM_STACK_SIZE)
#define CONFIGURE_LIBIO_MAXIMUM_FILE_DESCRIPTORS 32
#define CONFIGURE_INIT_TASK_PRIORITY	100
#define CONFIGURE_MAXIMUM_DRIVERS 16

/* #define CONFIGURE_INIT_TASK_INITIAL_MODES (RTEMS_PREEMPT | \ */
/*                                            RTEMS_TIMESLICE | \ */
/*                                            RTEMS_ASR | \ */
/*                                            RTEMS_INTERRUPT_LEVEL(0)) */

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
#define CONFIGURE_DRIVER_AMBAPP_GAISLER_PCIF    /* PCI is for RASTA-IO and GR-701 GRSPW */
#define CONFIGURE_DRIVER_AMBAPP_GAISLER_GRPCI   /* PCI is for RASTA-IO and GR-701 GRSPW */
#define CONFIGURE_DRIVER_PCI_GR_RASTA_IO        /* GR-RASTA-IO has three GRSPW SpaceWire cores */
#define CONFIGURE_DRIVER_PCI_GR_701             /* GR-701 has three GRSPW SpaceWire cores */

#ifdef LEON2
  /* PCI support for AT697 */
  #define CONFIGURE_DRIVER_LEON2_AT697PCI
  /* AMBA PnP Support for GRLIB-LEON2 */
  #define CONFIGURE_DRIVER_LEON2_AMBAPP
#endif

#define CONFIGURE_DRIVER_AMBAPP_GAISLER_GRSPW   /* GRSPW Driver */

#include <drvmgr/drvmgr_confdefs.h>

#include <rtems.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>

#undef ENABLE_NETWORK
/* Include driver configurations and system initialization */
#include "config.c"

#include <grspw.h>

/* Select GRSPW core to be used in sample application. 
 *  - /dev/grspw0              (First ON-CHIP core)
 *  - /dev/grspw1              (Second ON-CHIP core)
 *  - /dev/rastaio0/grspw0     (The GRSPW core on first GR-RASTA-IO board)
 *  - /dev/rastaio0/grspw1     (The second GRSPW core on first GR-RASTA-IO board)
 *  - /dev/gr701_0/grspw0      (The first GRSPW core on first GR-701 board)
 *  - /dev/gr701_1/grspw0      (The first GRSPW core on second GR-701 board)
 */
/*#define GRSPW_DEVICE_NAME1 "/dev/grspw0"
#define GRSPW_DEVICE_NAME2 "/dev/grspw1"*/
#define GRSPW_DEVICE_NAME1 "/dev/rastaio0/grspw0"
#define GRSPW_DEVICE_NAME2 "/dev/rastaio0/grspw1"

rtems_task task1(rtems_task_argument argument);
rtems_task task2(rtems_task_argument argument);

void print_config(spw_config *cnf);
void print_statistics(spw_stats *stats);
void check_init_config(spw_config *cnf);
char *link_status(int status);
extern int errno;

char *lstates[6] = {"Error-reset", 
                    "Error-wait",
                    "Ready",
                    "Started",
                    "Connecting",
                    "Run"
};

rtems_id   Task_id[3];         /* array of task ids */
rtems_name Task_name[3];       /* array of task names */

/* ========================================================= 
   initialisation */
rtems_task Init(
        rtems_task_argument ignored
)
{
        FILE *f; int fd;
        rtems_status_code status;

        printf("******** Starting RTEMS Spacewire test 2 ********\n");

        system_init();

        Task_name[1] = rtems_build_name( 'S', 'E', 'N', 'D' );
        Task_name[2] = rtems_build_name( 'R', 'E', 'C', 'V' );

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

#ifdef TASK_TX       
        status = rtems_task_start(Task_id[1], task1, 1);
#endif
#ifdef TASK_RX
        status = rtems_task_start(Task_id[2], task2, 2);
#endif
        
        status = rtems_task_delete(RTEMS_SELF);
}

unsigned int init1 = 0, init2 = 0;
static char tx_pkt[4096];
static char tx_hpkt[256];

#define NODE_ADR_TX 1
#define NODE_ADR_RX 2

#ifdef TASK_TX
/* =========================================================  
   sender task */
rtems_task task1(
        rtems_task_argument unused
) 
{
        rtems_status_code status;
        rtems_event_set out;
        int i;
        int j;
        int k;
        int fd;
        int lstatus;
        int stat;
				int ret;
        spw_ioctl_pkt_send *pkt = (spw_ioctl_pkt_send *)malloc(sizeof(spw_ioctl_pkt_send));
        spw_stats *statistics = (spw_stats *)malloc(sizeof(spw_stats));
        spw_config *config = (spw_config *)malloc(sizeof(spw_config));
        spw_ioctl_packetsize *ps = (spw_ioctl_packetsize *)malloc(sizeof(spw_ioctl_packetsize));
        
        printf("**** Transmit test task started ****\n");
        printf("\n");        
        printf("Opening " GRSPW_DEVICE_NAME1 ": ");
				j=0;
				fd = open(GRSPW_DEVICE_NAME1, O_RDWR);
				if ( fd < 0 ){
					printf("Failed to open " GRSPW_DEVICE_NAME1 " (%d)\n",errno);
					exit(0);
				}
        printf("opened successfully\n");
        printf("**** Checking initial configuration for " GRSPW_DEVICE_NAME1 " ****\n");
        printf("\n");
        if (ioctl(fd, SPACEWIRE_IOCTRL_GET_CONFIG, config) == -1) {
                printf("ioctl failed: SPACEWIRE_IOCTRL_GET_CONFIG \n");
                exit(0);
        }
        print_config(config);
        if (ioctl(fd, SPACEWIRE_IOCTRL_SET_NODEADDR, NODE_ADR_TX) == -1) {
                printf("ioctl failed: SPACEWIRE_IOCTRL_SET_NODEADDR \n");
                exit(0);
        }
        if (ioctl(fd, SPACEWIRE_IOCTRL_GET_CONFIG, config) == -1) {
                printf("ioctl failed: SPACEWIRE_IOCTRL_GET_CONFIG\n");
                exit(0);
        }
				if (ioctl(fd, SPACEWIRE_IOCTRL_SET_TXBLOCK, 1) == -1) {
                printf("ioctl failed: SPACEWIRE_IOCTRL_SET_TXBLOCK\n");
                exit(0);
        }
				
				printf("Trying to bring link up\n");
				while(1){
					if (ioctl(fd, SPACEWIRE_IOCTRL_START, 0) == -1) {
  	              sched_yield(); /*printf("ioctl failed: SPACEWIRE_IOCTRL_START\n");*/
      	  }else
						break;
				}
				printf("Link is up\n");
				
				printf("Disabling transmitter and receiver\n");
				if ( ioctl(fd, SPACEWIRE_IOCTRL_STOP, 0) == -1 ){
					printf("ioctl failed: SPACEWIRE_IOCTRL_STOP (%d)\n",errno);
					exit(0);
				}
				
				printf("Trying to bring link up again\n");
				while(1){
					if (ioctl(fd, SPACEWIRE_IOCTRL_START, 0) == -1) {
  	              sched_yield(); /*printf("ioctl failed: SPACEWIRE_IOCTRL_START\n");*/
      	  }else
						break;
				}
				printf("Link is up\n");
				
        if (ioctl(fd, SPACEWIRE_IOCTRL_SET_CLKDIV, 0) == -1) {
                printf("ioctl failed: SPACEWIRE_IOCTRL_SET_CLKDIV\n");
                exit(0);
        }
        
        print_config(config);
        if (ioctl(fd, SPACEWIRE_IOCTRL_GET_LINK_STATUS, &(lstatus)) == -1) {
                printf("ioctl failed: SPACEWIRE_IOCTRL_GET_LINK_STATUS\n");
                exit(0);
        }
        printf("Spacewire link 1 is in %s state\n", link_status(lstatus));
        /* printf("Set link error event id and link error irq\n"); */
/*                 printf("Task Id 2: %i \n", Task_id[1]); */
/*                 ioctl(fd, SPACEWIRE_IOCTRL_SET_EVENT_ID, Task_id[1]); */
/*                 ioctl(fd, SPACEWIRE_IOCTRL_SET_LINK_ERR_IRQ, 1); */
/*                 rtems_event_receive(RTEMS_ALL_EVENTS, RTEMS_WAIT | RTEMS_EVENT_ANY, RTEMS_NO_TIMEOUT, &out); */
/*                 printf("Got event 1\n"); */
/*                 ioctl(fd, SPACEWIRE_IOCTRL_SET_DISABLE_ERR, 1); */
/*                 rtems_event_receive(RTEMS_ALL_EVENTS, RTEMS_WAIT | RTEMS_EVENT_ANY, RTEMS_NO_TIMEOUT, &out); */
/*                 printf("Got event 2\n"); */
/*                 ioctl(fd, SPACEWIRE_IOCTRL_SET_LINK_ERR_IRQ, 0); */
/*                 ioctl(fd, SPACEWIRE_IOCTRL_LINKSTART, 1); */
/*                 printf("Link error test finished\n"); */
                /*config print finished, allow task 2 to print its config*/				
				i=0;
				while(1){
					printf("-------------- Sending Initiator ----------------\n");
					tx_pkt[0] = NODE_ADR_RX;
					tx_pkt[1] = 'G';
					tx_pkt[2] = 'A';
					tx_pkt[3] = 'I';
					tx_pkt[4] = 'S';
					tx_pkt[5] = 'L';
					tx_pkt[6] = 'E';
					tx_pkt[7] = 'R';
					tx_pkt[8] = ' ';
					tx_pkt[9] = 'S';
					tx_pkt[10] = 'P';
					tx_pkt[11] = 'W';
					tx_pkt[12] = '0'+i;
					tx_pkt[13] = 0;
					
					/* Send packet */
					if ( (ret=write(fd, tx_pkt, 14)) <= 0 ){
						printf("Write failed, errno: %d, ret: %d\n",errno,ret);
					}
					
					/* Sleep 5s */
					printf("Sleeping 1s\n");
					sleep(1);
					i++;
					if ( i>9 )
						i=0;
				}
}

#endif


#ifdef TASK_RX
/* ========================================================= 
   receiver task */

rtems_task task2(
        rtems_task_argument unused
) 
{
        rtems_status_code status;
        spw_config *config;
        spw_stats *statistics = (spw_stats *)malloc(sizeof(spw_stats));
        char *rx_pkt;
        char *cx_pkt;
        char *cx_hpkt;
        int fd;
        rtems_event_set event0, event1;
        int len;
        int i;
        int j;
        int k;
        int st;
        int lstatus;
        int rd;
        int tmp;
        
        rx_pkt = (char *)malloc(4096);
        cx_pkt = (char *)malloc(4096);
        cx_hpkt = (char *)malloc(256);
        config = (spw_config *)malloc(sizeof(spw_config));
        spw_ioctl_packetsize *ps = (spw_ioctl_packetsize *)malloc(sizeof(spw_ioctl_packetsize));

        printf(" **** Receive test task started ****\n ");
        printf("Opening " GRSPW_DEVICE_NAME2 ": ");
        j = 0;
				
				errno=0;
				if ((fd = open(GRSPW_DEVICE_NAME2, O_RDONLY)) < 0 ){
					printf("Failed to open " GRSPW_DEVICE_NAME2 ", errno: %d, ret: %d\n",errno,fd);
					exit(0);
				}
        printf("opened " GRSPW_DEVICE_NAME2 " successfully\n");

/* Make driver use the default timings as given by HW when reset */
#if 0				
				/* make driver calculate timings from system clock */
				if ( ioctl(fd,SPACEWIRE_IOCTRL_SET_COREFREQ,0) == -1 ){
				        printf("ioctl failed: SPACEWIRE_IOCTRL_SET_COREFREQ, errno: %d\n",errno);
                exit(0);
				}
        			
				/* make driver calculate timings from 40MHz spacewire clock */
				if ( ioctl(fd,SPACEWIRE_IOCTRL_SET_COREFREQ,40000) == -1 ){
				        printf("ioctl failed: SPACEWIRE_IOCTRL_SET_COREFREQ, errno: %d\n",errno);
                exit(0);
				}
#endif
        if (ioctl(fd, SPACEWIRE_IOCTRL_GET_CONFIG, config) == -1) {
                printf("ioctl failed: SPACEWIRE_IOCTRL_GET_CONFIG\n");
                exit(0);
        }
        printf(" Checking initial configuration for Spacewire 2 ****\n");
/*        check_init_config(config);*/
        print_config(config);				
				
        if (ioctl(fd, SPACEWIRE_IOCTRL_SET_NODEADDR, NODE_ADR_RX) == -1) {
                printf("ioctl failed: SPACEWIRE_IOCTRL_SET_NODEADDR\n");
                exit(0);
        }
				
        if (ioctl(fd, SPACEWIRE_IOCTRL_SET_RXBLOCK, 1) == -1) {
                printf("ioctl failed: SPACEWIRE_IOCTRL_SET_RXBLOCK\n");
                exit(0);
        }
				
				/* bring link up */
				printf("Trying to bring link up\n");
				while(1){
					if (ioctl(fd, SPACEWIRE_IOCTRL_START, 0) == -1) {
  	              sched_yield(); /*printf("ioctl failed: SPACEWIRE_IOCTRL_START\n");*/
      	  }else
						break;
				}
				printf("Link is up, Changing speed\n");		

				if (ioctl(fd, SPACEWIRE_IOCTRL_SET_CLKDIV, 0) == -1) {
                printf("ioctl failed: SPACEWIRE_IOCTRL_SET_CLKDIV\n");
                exit(0);
        }
				
        if (ioctl(fd, SPACEWIRE_IOCTRL_GET_CONFIG, config) == -1) {
                printf("ioctl failed: SPACEWIRE_IOCTRL_GET_CONFIG\n");
                exit(0);
        }
        print_config(config);
        if (ioctl(fd, SPACEWIRE_IOCTRL_GET_LINK_STATUS, &(lstatus)) == -1) {
                printf("ioctl failed: SPACEWIRE_IOCTRL_GET_LINK_STATUS\n");
                exit(0);
        }
        printf("Spacewire link 2 is in %s state\n", link_status(lstatus));
        
				while(1){
					printf("-------------- Receiving Initiator ----------------\n");
					tmp = 13;
					len = read(fd, rx_pkt, tmp);
					if ( len <= 0 ){
						printf("Read Failed, errno: %d, ret: %d\n",errno,len);
						sleep(1);
						continue;
					}
					
					/* handle message */
					printf("Got Message of length %d, to address %d\n",len,rx_pkt[0]);
					printf("Message data: [ 0x%x 0x%x ...] (%s)\n",rx_pkt[1],rx_pkt[2],&rx_pkt[0]);
				}
				close(fd);
}

#endif

/* ========================================================= 
   event task */

void print_config(spw_config *cnf) 
{
        printf("\n");
        printf(" ******** CONFIG ********  \n");
        printf("Node Address: %i\n", cnf->nodeaddr);
        printf("Destination Key: %i\n", cnf->destkey);
        printf("Clock Divider: %i\n", cnf->clkdiv);
        printf("Rx Maximum Packet: %i\n", cnf->rxmaxlen);
        printf("Timer: %i\n", cnf->timer);
        printf("Linkdisabled: %i\n", cnf->linkdisabled);
        printf("Linkstart: %i\n", cnf->linkstart);
        printf("Disconnect: %i\n", cnf->disconnect);
        printf("Promiscuous: %i\n", cnf->promiscuous);
        printf("RMAP Enable: %i\n", cnf->rmapen);
        printf("RMAP Buffer Disable: %i\n", cnf->rmapbufdis);
        printf("Check Rmap Error: %i\n", cnf->check_rmap_err);
        printf("Remove Protocol ID: %i\n", cnf->rm_prot_id);
        printf("Blocking Transmit: %i\n", cnf->tx_blocking);
        printf("Disable when Link Error: %i\n", cnf->disable_err);
        printf("Link Error IRQ Enabled: %i\n", cnf->link_err_irq);
        printf("Link Error Event Task ID: %i\n", cnf->event_id);
        printf("RMAP Available: %i\n", cnf->is_rmap);
        printf("RMAP CRC Available: %i\n", cnf->is_rmapcrc);
        printf("Unaligned Receive Buffer Support: %i\n", cnf->is_rxunaligned);
        printf("\n");
        
}

void print_statistics(spw_stats *stats) 
{
        printf("\n");
        printf(" ******** STATISTICS ********  \n");
        printf("Transmit link errors: %i\n", stats->tx_link_err);
        printf("Receiver RMAP header CRC errors: %i\n", stats->rx_rmap_header_crc_err);
        printf("Receiver RMAP data CRC errors: %i\n", stats->rx_rmap_data_crc_err);
        printf("Receiver EEP errors: %i\n", stats->rx_eep_err);
        printf("Receiver truncation errors: %i\n", stats->rx_truncated);
        printf("Parity errors: %i\n", stats->parity_err);
        printf("Escape errors: %i\n", stats->escape_err);
        printf("Credit errors: %i\n", stats->credit_err);
        printf("Disconnect errors: %i\n", stats->disconnect_err);
        printf("Write synchronization errors: %i\n", stats->write_sync_err);
        printf("Early EOP/EEP: %i\n", stats->early_ep);
        printf("Invalid Node Address: %i\n", stats->invalid_address);
        printf("Packets transmitted: %i\n", stats->packets_sent);
        printf("Packets received: %i\n", stats->packets_received);
}


void check_init_config(spw_config *cnf) 
{
   if (cnf->nodeaddr != 254) {
           printf("Incorrect initial node address! Expected: %i Got: %i\n", 254, cnf->nodeaddr);
           exit(0);
   }
   if (cnf->destkey != 0) {
           printf("Incorrect initial destination key! Expected: %i Got: %i\n", 0, cnf->destkey);
           exit(0);
   }
   if (cnf->rxmaxlen != 1024) {
           printf("Incorrect initial Rx maximum packet length! Expected: %i Got: %i\n", 1024 , cnf->rxmaxlen);
           exit(0);
   }
   if (cnf->linkdisabled != 0) {
           printf("Incorrect initial linkdisabled value! Expected: %i Got: %i\n", 0, cnf->linkdisabled);
           exit(0);
   }
   if (cnf->linkstart != 1) {
           printf("Incorrect initial linkstart value! Expected: %i Got: %i\n", 1, cnf->linkstart);
           exit(0);
   }
   if (cnf->promiscuous != 0) {
           printf("Incorrect initial promiscuous mode value! Expected: %i Got: %i\n", 0, cnf->promiscuous);
           exit(0);
   }
   if (cnf->check_rmap_err != 0) {
           printf("Incorrect initial check rmap error value! Expected: %i Got: %i\n", 0, cnf->check_rmap_err);
           exit(0);
   }
   if (cnf->rm_prot_id != 0) {
           printf("Incorrect initial remove protocol id value! Expected: %i Got: %i\n", 0, cnf->rm_prot_id);
           exit(0);
   }
   if (cnf->tx_blocking != 0) {
           printf("Incorrect initial tx blocking! Expected: %i Got: %i\n", 0, cnf->tx_blocking);
           exit(0);
   }
   if (cnf->disable_err != 0) {
           printf("Incorrect initial disable when link error value! Expected: %i Got: %i\n", 0, cnf->disable_err);
           exit(0);
   }
   if (cnf->link_err_irq != 0) {
           printf("Incorrect initial link error enable value! Expected: %i Got: %i\n", 0, cnf->link_err_irq);
           exit(0);
   }
   if (cnf->event_id != 0) {
           printf("Incorrect initial event id! Expected: %i Got: %i\n", 0, cnf->event_id);
           exit(0);
   }
}

char *link_status(int status) 
{
        return lstates[status];
       
}
