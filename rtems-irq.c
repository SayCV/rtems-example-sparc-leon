#include <rtems.h>
/* configuration information */

#define CONFIGURE_INIT

#include <bsp.h> /* for device driver prototypes */

rtems_task Init (rtems_task_argument argument);
rtems_isr handleExternalIrq (rtems_vector_number vector);

#define CONFIGURE_APPLICATION_NEEDS_CONSOLE_DRIVER
#define CONFIGURE_APPLICATION_NEEDS_CLOCK_DRIVER

#define CONFIGURE_MAXIMUM_TASKS             4

#define CONFIGURE_RTEMS_INIT_TASKS_TABLE

#define CONFIGURE_EXTRA_TASK_STACKS         (3 * RTEMS_MINIMUM_STACK_SIZE)

#include <rtems/confdefs.h>

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

    status = rtems_interrupt_catch (handleExternalIrq, 0x12, &old_handle);
    status = rtems_interrupt_catch (handleExternalIrq, 0x13, &old_handle);

#ifdef __erc32__
    *(unsigned long *) (0x1f8004c) = 0x7ff0;
    *(unsigned long *) (0x1f800d0) = 0x80000;
    *(unsigned long *) (0x1f80054) = 0x0c;
#elif defined(LEON2)
    *(unsigned long *) (0x80000090) = 0xc;
    *(unsigned long *) (0x80000098) = 0xc;
#elif defined(LEON3)
#error Example not intended for LEON3 CPU
#endif
    exit(0);
}

rtems_isr
handleExternalIrq
(
    rtems_vector_number vector
)
{
    printf ("External interrupt received with vector 0x%x\n", vector);
}				/* handleExternalIrq */

