#include <rtems.h>
/* configuration information */

#define CONFIGURE_INIT

rtems_task Init (rtems_task_argument argument);
rtems_isr handleTrap (rtems_vector_number vector);

#include <bsp.h> /* for device driver prototypes */

rtems_task Init (rtems_task_argument argument);
rtems_isr handleExternalIrq (rtems_vector_number vector);

#define CONFIGURE_APPLICATION_NEEDS_CONSOLE_DRIVER
#define CONFIGURE_APPLICATION_NEEDS_CLOCK_DRIVER

#define CONFIGURE_MAXIMUM_TASKS             4

#define CONFIGURE_RTEMS_INIT_TASKS_TABLE

#define CONFIGURE_EXTRA_TASK_STACKS         (3 * RTEMS_MINIMUM_STACK_SIZE)

#include <rtems/confdefs.h>

/* If --drvmgr was enabled during the configuration of the RTEMS kernel */
#ifdef RTEMS_DRVMGR_STARTUP
 #ifdef LEON3
  /* Add Timer and UART Driver for this example */
  #ifdef CONFIGURE_APPLICATION_NEEDS_CLOCK_DRIVER
   #define CONFIGURE_DRIVER_AMBAPP_GAISLER_GPTIMER
  #endif
  #ifdef CONFIGURE_APPLICATION_NEEDS_CONSOLE_DRIVER
   #define CONFIGURE_DRIVER_AMBAPP_GAISLER_APBUART
  #endif
 #endif

 #include <drvmgr/drvmgr_confdefs.h>
#endif


#include <stdlib.h>
#include <stdio.h>

rtems_task
Init
(
    rtems_task_argument argument
)
{
    rtems_status_code status;
    rtems_isr_entry old_handle;
    int tmp;

    status = rtems_interrupt_catch (handleTrap, 
    	SPARC_SYNCHRONOUS_TRAP(0x09), &old_handle);
    status = rtems_interrupt_catch (handleTrap, 
    	SPARC_SYNCHRONOUS_TRAP(0x90), &old_handle);


    tmp = *((int *) 0x00000); 	/* cause trap 0x09 */

    asm ("ta 0x10");		/* cause trap 0x90 */

    exit(0);
}

rtems_isr
handleTrap
(
    rtems_vector_number vector
)
{
    printf ("Caught synchronous trap with vector 0x%02x\n", 
    	SPARC_REAL_TRAP_NUMBER(vector));
}				/* handleTrap */

