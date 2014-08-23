/* B1553RT Driver interface demo.
 * 
 * Operates in two different modes (SIMPLE or DEFAULT). The SIMPLE test 
 * prints the received messages to the console and twists the received 
 * by adding a constant to the data and putting it into the transmit
 * subaddress (incremeting descriptor by 32).
 *
 * The DEFAULT mode is a bit more complex, 
 *  1. No console printing unless BC requests it (Statistics printing)
 *  2. Count RX mode code receive statistics
 *  3. Updates TX Subaddress 30 with the RX mode code statistics.
 *  4. Receives RX Subaddress 1..29 and updates the TX subaddress 1..19 with
 *     the received data adding a constant to each 16-bit word.
 *  5. Counts received and transmitted number of messages
 *  6. BC can issue REASTART or PRINT stats command.
 *
 * The DEFAULT mode can be used together with the BC2 test.
 *
 */

#include <rtems.h>
/* configuration information */

#define CONFIGURE_INIT

#include <bsp.h> /* for device driver prototypes */

rtems_task Init( rtems_task_argument argument);	/* forward declaration needed */

/* configuration information */

#define CONFIGURE_APPLICATION_NEEDS_CONSOLE_DRIVER
#define CONFIGURE_APPLICATION_NEEDS_CLOCK_DRIVER
#define CONFIGURE_MAXIMUM_TASKS                         32
#define CONFIGURE_RTEMS_INIT_TASKS_TABLE
#define CONFIGURE_EXTRA_TASK_STACKS                     (64 * RTEMS_MINIMUM_STACK_SIZE)
#define CONFIGURE_LIBIO_MAXIMUM_FILE_DESCRIPTORS        64
#define CONFIGURE_INIT_TASK_PRIORITY                    100
#define CONFIGURE_MAXIMUM_DRIVERS                       32

/* Select drivers used by the driver manager */
#if defined(RTEMS_DRVMGR_STARTUP) && defined(LEON3)
 /* Add Timer and UART Driver for this example */
 #ifdef CONFIGURE_APPLICATION_NEEDS_CLOCK_DRIVER
  #define CONFIGURE_DRIVER_AMBAPP_GAISLER_GPTIMER
 #endif
 #ifdef CONFIGURE_APPLICATION_NEEDS_CONSOLE_DRIVER
  #define CONFIGURE_DRIVER_AMBAPP_GAISLER_APBUART
 #endif
#endif
#define CONFIGURE_DRIVER_AMBAPP_GAISLER_B1553RT
#define CONFIGURE_DRIVER_AMBAPP_GAISLER_B1553BRM

#ifdef LEON2
  /* AMBA PnP Support for GRLIB-LEON2 */
  #define CONFIGURE_DRIVER_LEON2_AMBAPP
#endif

#include <rtems/confdefs.h>
#include <drvmgr/drvmgr_confdefs.h>

#include <rtems/rtems_bsdnet.h>
#include "networkconfig.h"

#include <stdio.h>
#include <stdlib.h>
#include <ambapp.h>
#include <ambapp_ids.h>

#include <drvmgr/ambapp_bus.h>
#include <b1553rt.h>

/* Force a specific B1553RT frequency */
/* #define SET_B1553RT_FREQ RT_FREQ_16MHZ */

/* Include driver configurations and system initialization */
#include "config.c"

/* Define SIMPLE_RT_PRINT_TASK to enable the simple RT task printing
 * the received RT messages.
 */
#undef SIMPLE_RT_PRINT_TASK

int b1553rt_init(char *device_node0, int brm);
int b1553rt_start(void);
void b1553rt_print_stats(void);

rtems_task Init(
  rtems_task_argument ignored
)
{
	int status;

	system_init();

	/*** B1553RT TEST ***/

	printf("******** Initiating B1553RT test ********\n");

	/* Try to open RT driver */
    printf("Opening B1553RT driver\n");
	status = b1553rt_init("/dev/b1553rt0", 0);
	if ( status == -1) {
		/* RT driver not opened successfully, probably because
		 * RT hardware not present, we try to open BRM driver 
		 * instead operating the BRM in RT mode.
		 */
        printf("Trying BRM driver in RT mode instead [IGNORE PREVIOUS ERROR IF YOU HAVE BRM CORE]\n");
		status = b1553rt_init("/dev/b1553brm0", 1);
	}
	if ( status ) {
		printf("Failed to init 1553 RT TEST\n");
		exit(-1);	
	}

	b1553rt_start();

	/* Loop and print */
	while(1) {
		b1553rt_print_stats();
		sleep(2);
	}

	exit(0);
}

#include <rtems.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <bsp.h> /* for device driver prototypes */
#include <stdio.h>
#include <stdlib.h>

#include <b1553rt.h>
/*#include <b1553brm.h>*/
#define BRM_SET_MODE    0
#define BRM_MODE_BC 0x0
#define BRM_MODE_RT 0x1
#define BRM_MODE_BM 0x2
#define BRM_MODE_BM_RT 0x3 /* both RT and BM */

rtems_task b1553rt_test_task(rtems_task_argument argument);
rtems_task b1553rt2_test_task(rtems_task_argument argument);

static rtems_id		tis;		/* array of task ids */
static rtems_name	tname; 	/* array of task names */
int chan;

static unsigned int b1553rt_rxcnt = 0;
static unsigned int b1553rt_txcnt = 0;
static unsigned int b1553rt_txprepcnt = 0;
static unsigned int b1553rt_rxmodecode = 0;
static unsigned int b1553rt_txmodecode = 0;
static unsigned int b1553_driver_brm = 0;

/* ========================================================= 
   initialisation */

int b1553rt_init(char *device_node0, int brm)
{
    rtems_status_code status;
		
		/* Open B1553RT driver */
    chan = open(device_node0, O_RDWR);
    if ( chan < 0 ) {
        printf("Failed to open %s driver (%d) [THIS MIGHT BE EXPECTED]\n", device_node0, errno);
        return -1;
    }
    b1553_driver_brm = brm;

    tname = rtems_build_name( 'R', 'T', '0', '0');
        
    status = rtems_task_create(
        tname, 1, RTEMS_MINIMUM_STACK_SIZE * 4,
        RTEMS_DEFAULT_MODES | RTEMS_TIMESLICE,
        RTEMS_DEFAULT_ATTRIBUTES, &tis
        );
        
    if ( status != RTEMS_SUCCESSFUL) {
        printf("Failed to create B1553 tasks\n");
        return -2;
    }

    return 0;
}

int b1553rt_start(void)
{
#ifdef SIMPLE_RT_PRINT_TASK
    rtems_status_code status = rtems_task_start(tis, b1553rt_test_task, 0);
#else
    rtems_status_code status = rtems_task_start(tis, b1553rt2_test_task, 0);
#endif
	
    if ( status != RTEMS_SUCCESSFUL ) 
        return -1;

    return 0;
}
#ifdef SIMPLE_RT_PRINT_TASK
rtems_task b1553rt_test_task(rtems_task_argument unused) 
{
    struct rt_msg msg;
    int i;
    char buf[1024];
    int ofs,ret;
    int msglen;
    unsigned int broad;
    printf("Starting RT task\n");
    
    while (1) {
            

        /* read 1 message a time */
        ret = read(chan, &msg, 1);
        if ( ret < 0 ) {
            perror("B1553RT read:");
        }

        if ( msg.desc >= 32 ) {
            printf("Message desc >= 32\n");
            continue;
        }

        printf("---------------  RT MESSAGE: -------------------\n");
        if (msg.miw & (1<<7)) {
            printf("Message error, desc: %d, miw:%x\n", msg.desc, msg.miw);
        }

        printf("desc: %d, miw: %x, time: %x\n",msg.desc, msg.miw, msg.time);

        msglen = (msg.miw >> 11) & 0x1f;
        ofs=0;
        for(i=0; i<msglen; i++){
            ofs += sprintf(buf+ofs,"0x%04x ",msg.data[i]);
        }
        printf("Data: %s\n",buf);
        printf("------------------------------------------------\n");

        /* reply with twisted data */
        for(i=0; i<msglen; i++)
            msg.data[i] = msg.data[i]+i;

        /* respond to incoming message by putting it into 
         * transmit sub address.
         */
        msg.desc += 32;
        if ( write(chan, &msg, 1) < 0 ) {
            puts("Error replying\n");
        }
    }
}

#else

#define TEST_RESTART 0xf555
#define TEST_PRINT   0xf556
#define RXMODES 12

struct mode_code_test {
    unsigned short   cnt;       /* Number of messages received on this mode code */
    unsigned short   last_data; /* Last data received */
};
struct mode_code_setup {
    unsigned short  mode_code;
    unsigned short  index;      /* Index in mode_code_tests */
    int             supported;
    int             data_byte;
    int             broadcast;
};

struct mode_code_test mode_code_tests[15];

struct mode_code_setup mode_code_setup[32] =
{
    {0x00, 0, 0, 0, 0},
    {0x01, 1, 1, 0, 1},
    {0x02, 2, 1, 0, 0},
    {0x03, 3, 1, 0, 1},
    {0x04, 4, 1, 0, 1},
    {0x05, 5, 1, 0, 1},
    {0x06, 6, 1, 0, 1},
    {0x07, 7, 1, 0, 1},
    {0x08, 0, 1, 0, 1},
    {0x09, -1, 0, -1, -1},
    {0x0a, -1, 0, -1, -1},
    {0x0b, -1, 0, -1, -1},
    {0x0c, -1, 0, -1, -1},
    {0x0d, -1, 0, -1, -1},
    {0x0e, -1, 0, -1, -1},
    {0x0f, -1, 0, -1, -1},
    {0x10, 8, 1, 1, 0},
    {0x11, 9, 1, 1, 1},
    {0x12, 10, 1, 1, 0},
    {0x13, 11, 1, 1, 0},
    {0x14, 12, 0, 1, 1},
    {0x15, 13, 0, 1, 1},
    {0x16, -1, 0, -1, -1},
    {0x17, -1, 0, -1, -1},
    {0x18, -1, 0, -1, -1},
    {0x19, -1, 0, -1, -1},
    {0x1a, -1, 0, -1, -1},
    {0x1b, -1, 0, -1, -1},
    {0x1c, -1, 0, -1, -1},
    {0x1d, -1, 0, -1, -1},
    {0x1e, -1, 0, -1, -1},
    {0x1f, -1, 0, -1, -1},
};

int index_to_modecode[RXMODES] =
{
    /* 0 */     1,
    /* 1 */     2,
    /* 2 */     3,
    /* 3 */     4,
    /* 4 */     5,
    /* 5 */     6,
    /* 6 */     7,
    /* 7 */     0x10,
    /* 8 */     0x11,
    /* 9 */     0x12,
    /* 10 */    0x13
};

rtems_task b1553rt2_test_task(rtems_task_argument unused) 
{
    struct rt_msg msg;
    int i;
    char buf[1024];
    int ofs,ret;
    int msglen, wc, restarted;
    int broad, rtaddr, mode;
    int modecode, index;

    printf("Starting RT task\n");
    
    if ( b1553_driver_brm ) {
        mode = BRM_MODE_RT;
        ioctl(chan, BRM_SET_MODE, &mode);
    }

    /* Enable BroadCast */
    broad = 1;
    ioctl(chan, RT_SET_BCE, &broad);

    /* Set RT address to 1 */
    rtaddr = 1;
    ioctl(chan, RT_SET_ADDR, &rtaddr);
#if 0
    /* Set RT address to 1 */
    vectorw = 1;
    ioctl(chan, RT_SET_VECTORW, &vectorw);
#endif

    restarted = 0;
restart:
    b1553rt_rxcnt = 0;
    b1553rt_txcnt = 0;
    b1553rt_txprepcnt = 0;
    b1553rt_rxmodecode = 0;
    b1553rt_txmodecode = 0;
    memset(mode_code_tests, 0, sizeof(mode_code_tests));

    while (1) {

        /* read 1 message a time */
        ret = read(chan, &msg, 1);
        if ( ret < 0 ) {
            perror("B1553RT read:");
        }

        if ( msg.desc < 32 ) {
            /* RX MESSAGE */
            b1553rt_rxcnt++;
        } else if ( msg.desc < 64 ) {
            /* TX MESSAGE */
            b1553rt_txcnt++;
            continue;
        } else if ( msg.desc < 128 ) {
            
            if ( msg.desc < 96 ) {
                /* RX MODECODE */
                b1553rt_rxmodecode++;
                modecode = msg.desc - 64;
            } else {
                /* TX MODECODE */
                b1553rt_txmodecode++;
                modecode = msg.desc - 96;
            }

            /* Check sync with data for command */

            if ( modecode == 0x11 ) {
                /* SYNC WITH DATA */
                switch ( msg.data[0] ) {
                    case TEST_RESTART:
                        printf("RESTARTING\n");
                        restarted = 1;
                        goto restart;
                    case TEST_PRINT:
                        printf("MSGS: RX: %u, TX: %u (%u, %u, %u)\n", b1553rt_rxcnt, b1553rt_txcnt, b1553rt_txprepcnt, b1553rt_rxmodecode, b1553rt_txmodecode);
                        continue;
                }
            }

            index = mode_code_setup[modecode].index;
            if ( index == -1 ) {
                printf("INVALID INDEX FOR MODECODE 0x%x\n", modecode);
                exit(-1);
            }

            if ( mode_code_setup[modecode].supported == 0 ) {
                printf("MODECODE 0x%x NOT SUPPORTED (0x%x, %d, %d)\n", modecode, msg.desc, b1553rt_rxmodecode,b1553rt_txmodecode);
                exit(-1);
            }

            mode_code_tests[index].cnt++;
            if (  mode_code_setup[modecode].data_byte ) {
                mode_code_tests[index].last_data = msg.data[0];
            }

            /* Mode code 0x13 is interpreted as Update transmission area, it
             * is the last mode code sent by the BC.
             */
            if ( modecode == 0x13 ) {
                memcpy(&msg.data[0], mode_code_tests, sizeof(mode_code_tests));

                /* Update the transmit sub address 30 with modecode RX status */
                msg.desc = 30+32;
                msg.miw = (sizeof(mode_code_tests)/2) << 11; /* 32 words */
                if ( write(chan, &msg, 1) < 0 ) {
                    puts("Error replying\n");
                }
            }

            continue;
        } else {
            printf("INVALID DESCRIPTOR 0x%x\n", msg.desc);
            exit(-1);
        }

        if ( restarted == 0 ) {
            /* Must be resetted once */
            printf("NOT RESTARTED BEFORE RECEIVING MESSAGES\n");
            exit(-1);
        }

        /* Get Message length */
        msglen = (msg.miw >> 11) & 0x1f;
        if ( msglen == 0 ) 
            msglen = 32;

        /* Print message */
#if 0
        printf("---------------  RT MESSAGE: -------------------\n");
        if (msg.miw & (1<<7)) {
            printf("Message error, desc: %d, miw:%x\n", msg.desc, msg.miw);
        }

        printf("desc: %d, miw: %x, time: %x\n",msg.desc, msg.miw, msg.time);

        msglen = (msg.miw >> 11) & 0x1f;
        ofs=0;
        for(i=0; i<msglen; i++){
            ofs += sprintf(buf+ofs,"0x%04x ",msg.data[i]);
        }
        printf("Data: %s\n",buf);
        printf("------------------------------------------------\n");
#endif

        /* reply with twisted data */
        for(i=0; i<msglen; i++)
            msg.data[i] = msg.data[i] + i;

        /* respond to incoming message by putting it into 
         * transmit sub address.
         */
        msg.desc += 32;
        if ( write(chan, &msg, 1) < 0 ) {
            puts("Error replying\n");
        }
        b1553rt_txprepcnt++;
    }
}

#endif

void b1553rt_print_stats(void)
{
/*
    printf("MSGS: RX: %u, TX: %u (%u, %u, %u)\n", b1553rt_rxcnt, b1553rt_txcnt, b1553rt_txprepcnt, b1553rt_rxmodecode, b1553rt_txmodecode);
*/
}
