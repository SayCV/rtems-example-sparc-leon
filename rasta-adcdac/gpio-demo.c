/* Simple GPIOLIB interface example.
 *
 * --------------------------------------------------------------------------
 *  --  This file is a part of GAISLER RESEARCH source code.
 *  --  Copyright (C) 2009, Aeroflex Gaisler AB - all rights reserved.
 *  --
 *  -- ANY USE OR REDISTRIBUTION IN PART OR IN WHOLE MUST BE HANDLED IN
 *  -- ACCORDANCE WITH THE GAISLER LICENSE AGREEMENT AND MUST BE APPROVED
 *  -- IN ADVANCE IN WRITING.
 *  --
 *  -- BY DEFAULT, DISTRIBUTION OR DISCLOSURE IS NOT PERMITTED.
 *  --------------------------------------------------------------------------
 */

#include <rtems.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#include <gpiolib.h>

rtems_id gpio_sem;

void *portA, *portB;
struct gpiolib_config port_config;

volatile int gpioTestIsrStatus, gpioTestIsrStatusErrCnt, gpioTestIsrStopCnt, gpioTestIsrCnt;

void gpioTestIsrA(int irq, void *arg)
{
	gpioTestIsrCnt++;

	if ( gpioTestIsrStatus < 0 ) {
		gpioTestIsrStatusErrCnt++;
		rtems_semaphore_release(gpio_sem);
		return;
	}

	if ( arg != portA ) {
		gpioTestIsrStatus = -10;
		rtems_semaphore_release(gpio_sem);
		return;
	}
	if ( gpioTestIsrStopCnt == 0 ) {
		/* Stop IRQs by disabling IRQ on portA */
		gpioTestIsrStatus = -11; /* Done counting */
		port_config.mask = 0;
		gpiolib_set_config(portA, &port_config);
		gpiolib_irq_disable(portA);
		rtems_semaphore_release(gpio_sem);
		return;
	}
	gpioTestIsrStopCnt--;
}

int gpioTestInternal(char *A, char *B, int testInt)
{
	int val, err;
	int status;

	gpio_sem = 0;

	/*** OPEN PORTS ***/
	portA = gpiolib_open_by_name(A);
	if ( portA == NULL ) 
		return -1;

	portB = gpiolib_open_by_name(B);
	if ( portB == NULL ) 
		return -2;

	/*** Set up both ports ***/
	if ( gpiolib_set(portA, 0, 0) )
		return -3;
	if ( gpiolib_set(portB, 0, 0) )
		return -4;

	/* Configure interrupt disabled */
	port_config.mask = 0;
	port_config.irq_level = GPIOLIB_IRQ_LEVEL;
	port_config.irq_polarity = GPIOLIB_IRQ_POL_LOW;
	if ( gpiolib_set_config(portA, &port_config) )
		return -5;
	if ( gpiolib_set_config(portB, &port_config) )
		return -6;

	gpioTestIsrStatus = -1;
	gpioTestIsrStatusErrCnt = 0;
	gpioTestIsrStopCnt = 0;
	gpioTestIsrCnt = 0;

	/* Register interrupt handler for both ports */
	if ( testInt ) {
		if ( gpiolib_irq_register(portA, gpioTestIsrA, (void *)portA) )
			return -9;
	}

	/*** Test Ouput Low Level ***/
	if ( gpiolib_set(portA, 1, 0) )
		return -10;
	rtems_task_wake_after(2);
	val = 0x44556677;
	if ( gpiolib_get(portB, &val) )
		return -11;
	if ( val != 0 )
		return -12;

	/*** Test Ouput High Level ***/
	if ( gpiolib_set(portA, 1, 1) )
		return -20;
	rtems_task_wake_after(2);
	val = 0x44556677;
	if ( gpiolib_get(portB, &val) )
		return -21;
	if ( val != 1 )
		return -22;

	/*** Test Input Low Level ***/
	if ( gpiolib_set(portA, 0, 0) )
		return -30;
	if ( gpiolib_set(portB, 1, 0) )
		return -31;
	rtems_task_wake_after(2);
	val = 0x99887766;
	if ( gpiolib_get(portA, &val) )
		return -32;
	if ( val != 0 )
		return -33;

	/*** Test Input Low Level ***/
	if ( gpiolib_set(portB, 1, 1) )
		return -40;
	rtems_task_wake_after(2);
	val = 0x99887766;
	if ( gpiolib_get(portA, &val) )
		return -41;
	if ( val != 1 )
		return -42;

	if ( testInt ) {
		/*** Test Interrupt Low Level ***/
		if ( gpiolib_set(portA, 0, 0) )
			return -60;
		if ( gpiolib_set(portB, 1, 1) )
			return -61;
		rtems_task_wake_after(2);

		port_config.mask = 1; /* Unmask IRQ */
		port_config.irq_level = GPIOLIB_IRQ_LEVEL;
		port_config.irq_polarity = GPIOLIB_IRQ_POL_LOW;
		if ( gpiolib_set_config(portA, &port_config) )
			return -63;

		/* Set up interrupt before starting */
		gpioTestIsrStatus = 0;
		gpioTestIsrStatusErrCnt = 0;
		gpioTestIsrStopCnt = 63; /* 64 interrupts */
		gpioTestIsrCnt = 0;

		/* GPIO Semaphore created with count = 0 (will lock first time to semaphore obtain) */
		if ( rtems_semaphore_create(rtems_build_name('G', 'P', 'I', 'O'),
			0,
			RTEMS_FIFO|RTEMS_SIMPLE_BINARY_SEMAPHORE|RTEMS_NO_INHERIT_PRIORITY| \
			RTEMS_LOCAL|RTEMS_NO_PRIORITY_CEILING, 
			0,
			&gpio_sem) != RTEMS_SUCCESSFUL ) {
			printf("Failed to create GPIO semaphore, errno: %d\n",errno);
			return -1;
		}
		if ( gpiolib_irq_enable(portA) )
			return -66;
		rtems_task_wake_after(2);
		if ( gpioTestIsrCnt != 0 ) 
			return -67;
		/* Trigger 64 interrupts by driving low */
		if ( gpiolib_set(portB, 1, 0) )
			return -68;

		/* Wait for IRQ */
		err = 0;
		/* Wait 2 ticks until timeout */
		status = rtems_semaphore_obtain(gpio_sem, RTEMS_WAIT, 2);
		if ( status != RTEMS_SUCCESSFUL ) {
			err = -70;
		}
		if ( (gpioTestIsrCnt != 64) ||
		     (gpioTestIsrStatus != -11) ||
		     (gpioTestIsrStatusErrCnt != 0) ||
		     (gpioTestIsrStopCnt != 0) ) {
			printf("Interrupt test failed: %d,%d,%d,%d\n", gpioTestIsrCnt,
				gpioTestIsrStatus, gpioTestIsrStatusErrCnt, gpioTestIsrStopCnt);
			if ( err == 0 )
				err = -71;
		}
		if ( err ) {
			gpiolib_irq_disable(portA);
			return err;
		}

		/*** Test Interrupt High Level ***/
		port_config.mask = 1; /* Unmask IRQ */
		port_config.irq_level = GPIOLIB_IRQ_LEVEL;
		port_config.irq_polarity = GPIOLIB_IRQ_POL_HIGH;
		if ( gpiolib_set_config(portA, &port_config) )
			return -80;

		gpioTestIsrStatus = 0;
		gpioTestIsrStatusErrCnt = 0;
		gpioTestIsrStopCnt = 63; /* 64 interrupts */
		gpioTestIsrCnt = 0;
		if ( gpiolib_irq_enable(portA) )
			return -81;
		rtems_task_wake_after(2);
		if ( gpioTestIsrCnt != 0 )
			return -82;
		rtems_semaphore_obtain(gpio_sem, RTEMS_NO_WAIT, 0);
		rtems_semaphore_obtain(gpio_sem, RTEMS_NO_WAIT, 0);

		/* Trigger 64 interrupts by driving high */
		if ( gpiolib_set(portB, 1, 1) )
			return -83;
		rtems_task_wake_after(2);
		/* Wait for IRQ */
		err = 0;
		status = rtems_semaphore_obtain(gpio_sem, RTEMS_WAIT, 2);
		if ( status != RTEMS_SUCCESSFUL ) {
			err = -90;
		}
		if ( (gpioTestIsrCnt != 64) ||
		     (gpioTestIsrStatus != -11) ||
		     (gpioTestIsrStatusErrCnt != 0) ||
		     (gpioTestIsrStopCnt != 0) ) {
			printf("Interrupt test failed: %d,%d,%d,%d\n", gpioTestIsrCnt,
				gpioTestIsrStatus, gpioTestIsrStatusErrCnt, gpioTestIsrStopCnt);
			if ( err == 0 )
				err = -91;
		}
		if ( err ) {
			gpiolib_irq_disable(portA);
			return err;
		}

		/*** Test Interrupt Falling Edge ***/
		port_config.mask = 1; /* Unmask IRQ */
		port_config.irq_level = GPIOLIB_IRQ_EDGE;
		port_config.irq_polarity = GPIOLIB_IRQ_POL_LOW;
		if ( gpiolib_set_config(portA, &port_config) )
			return -100;

		gpioTestIsrStatus = 0;
		gpioTestIsrStatusErrCnt = 0;
		gpioTestIsrStopCnt = 0; /* Expect 1 interrupt */
		gpioTestIsrCnt = 0;
		if ( gpiolib_irq_enable(portA) )
			return -101;
		rtems_task_wake_after(2);
		if ( gpioTestIsrCnt != 0 )
			return -102;
		rtems_semaphore_obtain(gpio_sem, RTEMS_NO_WAIT, 0);
		rtems_semaphore_obtain(gpio_sem, RTEMS_NO_WAIT, 0);

		/* Trigger 64 interrupts by driving low */
		if ( gpiolib_set(portB, 1, 0) )
			return -103;
		rtems_task_wake_after(2);
		/* Wait for IRQ */
		err = 0;
		status = rtems_semaphore_obtain(gpio_sem, RTEMS_WAIT, 2);
		if ( status != RTEMS_SUCCESSFUL ) {
			err = -110;
		}
		if ( (gpioTestIsrCnt != 1) ||
		     (gpioTestIsrStatus != -11) ||
		     (gpioTestIsrStatusErrCnt != 0) ||
		     (gpioTestIsrStopCnt != 0) ) {
			printf("Interrupt test failed: %d,%d,%d,%d\n", gpioTestIsrCnt,
				gpioTestIsrStatus, gpioTestIsrStatusErrCnt, gpioTestIsrStopCnt);
			if ( err == 0 )
				err = -111;
		}
		if ( err ) {
			gpiolib_irq_disable(portA);
			return err;
		}

		/*** Test Interrupt Rising Edge ***/
		port_config.mask = 1; /* Unmask IRQ */
		port_config.irq_level = GPIOLIB_IRQ_EDGE;
		port_config.irq_polarity = GPIOLIB_IRQ_POL_HIGH;
		if ( gpiolib_set_config(portA, &port_config) )
			return -120;

		gpioTestIsrStatus = 0;
		gpioTestIsrStatusErrCnt = 0;
		gpioTestIsrStopCnt = 0; /* Expect 1 interrupt */
		gpioTestIsrCnt = 0;
		if ( gpiolib_irq_enable(portA) )
			return -121;
		rtems_task_wake_after(2);
		if ( gpioTestIsrCnt != 0 )
			return -122;
		rtems_semaphore_obtain(gpio_sem, RTEMS_NO_WAIT, 0);
		rtems_semaphore_obtain(gpio_sem, RTEMS_NO_WAIT, 0);

		/* Trigger 64 interrupts by driving high */
		if ( gpiolib_set(portB, 1, 1) )
			return -123;
		rtems_task_wake_after(2);
		/* Wait for IRQ */
		err = 0;
		status = rtems_semaphore_obtain(gpio_sem, RTEMS_WAIT, 2);
		if ( status != RTEMS_SUCCESSFUL ) {
			err = -130;
		}
		if ( (gpioTestIsrCnt != 1) ||
		     (gpioTestIsrStatus != -11) ||
		     (gpioTestIsrStatusErrCnt != 0) ||
		     (gpioTestIsrStopCnt != 0) ) {
			printf("Interrupt test failed: %d,%d,%d,%d\n", gpioTestIsrCnt,
				gpioTestIsrStatus, gpioTestIsrStatusErrCnt, gpioTestIsrStopCnt);
			if ( err == 0 )
				err = -131;
		}
		if ( err ) {
			gpiolib_irq_disable(portA);
			return err;
		}
	}
	return 0;
}

int gpioTest(char *A, char *B, int intTest)
{
	int ret;

	ret = gpioTestInternal(A, B, intTest);

	if ( portA )
		gpiolib_close(portA);
	if ( portB )
		gpiolib_close(portB);
	if ( gpio_sem ) {
		rtems_semaphore_delete(gpio_sem);
		gpio_sem = 0;
	}
	if ( ret )
		printf("gpioTest(%s,%s) Failed: %d\n", A, B, ret);

	return ret;
}

struct gpio_matrix_entry {
	char	*A;
	char	*B;
	int	intTest; /* 0=no IRQ, 3=A and B IRQ test, 1=A IRQ test, 2=B IRQ test */
};

/* Test Matrix for SpW cable */
struct gpio_matrix_entry gpio_test_matrix[] =
{
	{"/dev/rastaadcdac0/grgpio0/1", "/dev/rastaadcdac1/grgpio0/7", 3},
	{"/dev/rastaadcdac0/grgpio0/3", "/dev/rastaadcdac1/grgpio0/6", 3},
	{"/dev/rastaadcdac0/grgpio0/4", "/dev/rastaadcdac1/grgpio0/5", 3},
	{"/dev/rastaadcdac0/grgpio0/5", "/dev/rastaadcdac1/grgpio0/4", 3},
	{"/dev/rastaadcdac0/grgpio0/6", "/dev/rastaadcdac1/grgpio0/3", 3},
	{NULL, NULL, 0} /* END TEST MATRIX */
};

int gpioTestMatrix(void)
{
	struct gpio_matrix_entry *entry;
	int gpio_cnt, status;
	
	printf("Starting GPIO Test\n");
	
	gpio_cnt = 0;
	entry = &gpio_test_matrix[0];
	while ( (entry->A != NULL) && (entry->B != NULL) ) {
		status = gpioTest(entry->A, entry->B, entry->intTest & 1);
		if ( status ) {
			printf("gpioTestMatrix: Failed on entry %d (%s,%s): %d\n",
				gpio_cnt, entry->A, entry->B, status);
			return -1;
		}

		/* Test Opposite direction also */
		status = gpioTest(entry->B, entry->A, entry->intTest & 2);
		if ( status ) {
			printf("gpioTestMatrix: Failed on entry %d (%s,%s): %d\n",
				gpio_cnt, entry->B, entry->A, status);
			return -2;
		}

		entry++;
		gpio_cnt++;
	}

	printf("gpioTestMatrix: Tested Completed Successfully on %d port pairs\n", gpio_cnt);

	return 0;
}
