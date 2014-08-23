/* RTEMS sample driver for the memscrub AHB scrubber/status register IP core. 
 * 
 * Author: Magnus Hjorth, Aeroflex Gaisler
 * Contact: support@gaisler.com
 */

/*-----------------------------------------------------------------------
 * Operation:
 *
 * This driver sets up the memscrub device to repeatedly scrub a
 * memory area. If multiple correctable errors occur during the same
 * scrubber iteration, the scrubber is switched into regeneration
 * mode. When regeneration has completed, the ordinary scrubbing is
 * resumed.
 *
 * The driver can be used in two modes, a "managed" mode where we
 * get an IRQ on every single error on the AHB bus and every completed
 * scrub run, and a silent mode where it sets up the scrubber to loop
 * in the background and only interrupt after multiple correctable
 * errors condition or on an uncorrectable error. The driver is
 * completely interrupt driven, and in the silent mode it does not
 * consume any CPU time during normal scrubbing operation.
 *
 * An RTEMS message queue is used to communicate status information
 * from the driver's interrupt handler to the user. Depending on mode,
 * different amount of messages will be sent. The driver ignores errors 
 * resulting from the queue being full, therefore the user is not 
 * required to read out the queue. The application needs to provide 
 * room for the message queue via the configuration table.
 *
 * Configuration:
 *
 * The driver uses Aeroflex Gaisler's RTEMS Driver Manager
 * framework. To use the driver, the registration function should be
 * either called manually from the init task or added to the global
 * list of drivers (via drvmgr_confdefs.h). The driver manager takes
 * care of probing the on-chip bus for the hardware. 
 *
 * If more than one memscrub device is present, they are addressed using a
 * serial index from 0...N-1
 *
 * The driver is configured via a number of driver resources:
 *   "memstart","memsize":        Determines the memory area to scrub
 *   "autostart":                 Set to 1 to start scrubbing in the 
 *                                background automatically when the driver
 *                                is initialized. (default: 0)
 *   "opermode":                  Operating mode, 0=silent, 1=managed 
 *                                (default: 0)
 *   "regenthres":                Number of errors before regeneration mode
 *                                is entered (default: 5)
 *   "scrubdelay","regendelay":   Delay time in cycles between processing 
 *                                bursts in scrub/regeneration modes.
 *                                (default: 100 scrub / 10 regen)
 *-----------------------------------------------------------------------
 */

#ifndef MEMSCRUB_RTEMS_H_INCLUDED
#define MEMSCRUB_RTEMS_H_INCLUDED


#include <drvmgr/drvmgr.h>
#include <drvmgr/ambapp_bus.h>

#ifndef GAISLER_MEMSCRUB
#define GAISLER_MEMSCRUB 0x57
#endif

#define DRIVER_AMBAPP_GAISLER_MEMSCRUB_ID DRIVER_AMBAPP_ID(VENDOR_GAISLER, GAISLER_MEMSCRUB)

/* Resources required for driver message queue */
#define MEMSCRUB_MAXIMUM_MESSAGE_QUEUES 1
#define MEMSCRUB_MESSAGE_BUFFER_MEMORY CONFIGURE_MESSAGE_BUFFERS_FOR_QUEUE(8,8)

/* Driver registration function */
void memscrubr_register(void);

/* Returns number of devices found */
int memscrubr_count(void);

/* Start/stop scrubber operation */
int memscrubr_start(int index);
int memscrubr_stop(int index);

/* Override one of the resource values given above.
 * Should be called when the scrubber is stopped. */
int memscrubr_set_option(int index, char *resname, int value);

/* Message structure */
struct memscrubr_message {
  /* 0=None, 1=Run done, 2=Error detected, 3=Starting regeneration */
  int msgtype; 
  union {
    struct {
      int cecount;
    } done; /* Type 1 */
    struct {
      int errtype; /* 1=UE, 2=CE */
      int addr;
      int master;
      int hwrite;
      int hsize;
    } err; /* Type 2 */
  } d;
};

/* Fetch a message from the driver's message queue and return it in msgout.
 * If no message is available, msgout->type is set to 0 (none)
 * If block=1, the calling task is blocked until a message is available */
int memscrubr_get_message(int index, int block, 
			  struct memscrubr_message *msgout);

/* Print a debug/status message to the console */
int memscrubr_print_status(int index);

/* Read out the total number of correctable/uncorrectable errors encountered */
int memscrubr_get_totals(int index, int totals[2]);

#endif
