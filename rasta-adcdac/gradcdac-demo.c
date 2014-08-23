/* Simple GGRADCDAC interface example.
 *
 * --------------------------------------------------------------------------
 *  --  This file is a part of GAISLER RESEARCH source code.
 *  --  Copyright (C) 2008, Gaisler Research AB - all rights reserved.
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
#include <string.h>
#include <unistd.h>

#include "gradcdac.h"

/* Define this to use interrupt instead of a timeout to signal
 * when adc is done.
 */
#define ADC_USE_INTERRUPT

/* Define this to print information about interrupts */
#define ADC_SHOW_INTS

#define MAX_CHANS 4

void adcdac_task1(int argument);

/* ADC/DAC Handles */
void *adc_hand;
void *dac_hand;

/**** Statistics ****/
volatile unsigned int adc_ints;       /* Number of ADC interrupts handled */
volatile unsigned int dac_ints;       /* Number of DAC interrupts handled */
volatile unsigned int adcdac_wakeups; /* Number times the ADC/DAC task has 
                                      * been waked by the ADC/DAC interrupt. */
volatile unsigned int adc_last_val[MAX_CHANS];
volatile unsigned int adc_filtered_val[MAX_CHANS];

/* Binary Semaphore used to wake task when an ADC/DAC interrupt 
 * has been taken.
 */
rtems_id   adcdac_sem;
rtems_id   adcdac_tid;         /* Task ID */
rtems_name adcdac_tname;       /* Task name */

/* ========================================================= 
   initialisation */

int adcdac_init(char *adc_devname, char *dac_devname)
{
	int chan;
	int status;

	if ( !adc_devname && !dac_devname ) {
		printf("Invalid argument\n");
		return -1;
	}
	if ( !dac_devname ) {
		dac_devname = adc_devname;
	} else if ( !adc_devname ) {
		adc_devname = dac_devname;
	}

	printf("******** Initiating GRADCDAC test ********\n");
	adc_ints=0;
	dac_ints=0;
	adcdac_wakeups=0;
	for(chan=0; chan<MAX_CHANS; chan++) {
		adc_last_val[chan]=0;
		adc_filtered_val[chan]=0;
	}

	printf("Opening ADC/DAC channels\n");

	/* Open ADC */
	adc_hand = gradcdac_open(adc_devname);
	if ( adc_hand == NULL ) {
		printf("## Failed to open ADC channel\n");
		return -1;
	}

	if ( strcmp(adc_devname, dac_devname) == 0 ) {
		/* The same GRADCDAC device for DAC as for ADC */
		dac_hand = adc_hand;
	} else {
		/* Open DAC */
		dac_hand = gradcdac_open(dac_devname);
		if ( dac_hand == NULL ) {
			printf("## Failed to open DAC channel\n");
			return -1;
		}
	}

	/* ADC/DAC Semaphore created with count = 0 (will lock first time to semaphore obtain) */
	if ( rtems_semaphore_create(rtems_build_name('A', 'D', 'E', 'M'),
		0,
		RTEMS_FIFO|RTEMS_SIMPLE_BINARY_SEMAPHORE|RTEMS_NO_INHERIT_PRIORITY| \
		RTEMS_LOCAL|RTEMS_NO_PRIORITY_CEILING, 
		0,
		&adcdac_sem) != RTEMS_SUCCESSFUL ) {
		printf("Failed to create ADC/DAC semaphore, errno: %d\n",errno);
		return -1;
	}

	adcdac_tname = rtems_build_name( 'A', 'D', 'T', 'K' );

        status = rtems_task_create(
                adcdac_tname, 1, RTEMS_MINIMUM_STACK_SIZE * 4,
                RTEMS_DEFAULT_MODES, 
                RTEMS_DEFAULT_ATTRIBUTES, &adcdac_tid
                );

	return 0;
}

int adcdac_start(void)
{
	int status;

	printf("Starting ADC/DAC task\n");

	status = rtems_task_start(adcdac_tid, adcdac_task1, 1);
	if ( status != RTEMS_SUCCESSFUL ) {
		printf("Failed to start ADC/DAC task\n");
		return -1;
	}
	return 0;
}

void adc_isr(void *cookie, void *arg)
{
	adc_ints++;

	rtems_semaphore_release(adcdac_sem);
}

void dac_isr(void *cookie, void *arg)
{
	dac_ints++;
}

/* Filter 10 adc values by time weighting:
 * Sample time (0 most recent) | weight
 * 0: 45%
 * 1: 15%
 * 2: 10%
 * 3: 10%
 * 4: 5%
 * 5: 5%
 * 6: 5%
 * 7: 2%
 * 8: 2%
 * 9: 1%
 */
unsigned int adc_calc(unsigned short *adcvals)
{
	unsigned int adc_filt_val=0;
	int i;
	static unsigned int weight[10] =
	{
		4500,
		1500,
		1000,
		1000,
		500,
		500,
		500,
		200,
		200,
		100
	};

	for(i=0; i<10; i++){
		adc_filt_val = adc_filt_val + ((unsigned int)adcvals[i])*weight[i];
	}
	adc_filt_val = adc_filt_val / 10000;
	return adc_filt_val;
}

/* ADC/DAC Task */
void adcdac_task1(
        int argument
) 
{
	struct gradcdac_config cfg;
	unsigned short adc_value, adcvals[MAX_CHANS][10];
	int ret;
	int i=0;
	unsigned int status, oldstatus;
	int chan, chansel;

	memset(adcvals,0,sizeof(adcvals));

	printf("ADC/DAC task running\n");

#ifdef ADC_USE_INTERRUPT  
	/* Connect ADC/DAC to Interrupt service routine 'adcdac_isr' */  
	gradcdac_install_irq_handler(adc_hand, GRADCDAC_ISR_ADC, adc_isr, 0);
	gradcdac_install_irq_handler(dac_hand, GRADCDAC_ISR_DAC, dac_isr, 0);
#endif

	/* Get ADC/DAC configuration */
	gradcdac_get_config(adc_hand, &cfg);

	/* Change ADC/DAC Configuration 0xfafc42
	 */

	cfg.dac_ws = 0x1f;
	cfg.wr_pol = 0;
	cfg.dac_dw = 2;
	cfg.adc_ws = 0x1f;
	cfg.rc_pol = 1;
	cfg.cs_mode = 0;
	cfg.cs_pol = 0;
	cfg.ready_mode = 1;
	cfg.ready_pol = 0;
	cfg.trigg_pol = 0;
	cfg.trigg_mode = 0;
	cfg.adc_dw = 2;

	/* Set ADC/DAC Configurations */
	gradcdac_set_config(adc_hand, &cfg);

	printf("Making DAC conversion\n");

	/* Convert a Digital sample to an analogue Value,
	 * in this test, the ADC input may be connected 
	 * to the DAC output.
	 *
	 * First the Address bit 6 (12/8) must be set. 
	 *
	 */
	gradcdac_set_dataoutput(adc_hand, 0);
	gradcdac_set_datadir(adc_hand, 0);
	gradcdac_set_adrdir(adc_hand, 0);
	gradcdac_set_adrdir(adc_hand, 0xff);
	gradcdac_set_adroutput(adc_hand, 0x40);
	gradcdac_dac_convert(adc_hand, 0x400);

	/* Wait for DAC to finish */
	status = 0;
	do {
		oldstatus = status;
		rtems_task_wake_after(1);
		status = gradcdac_get_status(adc_hand);
	} while ( gradcdac_DAC_isOngoing(status) );

	if ( gradcdac_DAC_ReqRej(status) ) {
		printf("DAC: rejected\n");
		exit(-1);
	}

	if ( !gradcdac_DAC_isCompleted(status) ) {
		printf("DAC: didn't complete 0x%x prev 0x%x\n", status, oldstatus);
		status = gradcdac_get_status(adc_hand);
		printf("DAC: Status afterwards: 0x%x\n", status);
		exit(-1);
	}

	printf("DAC conversion complete\n");

	chan=0;

	while (1) {

#ifdef ADC_USE_INTERRUPT
		rtems_task_wake_after(20);
/*		sleep(1);*/

		/* Clear semaphore */
		rtems_semaphore_obtain(adcdac_sem, RTEMS_NO_WAIT, RTEMS_NO_TIMEOUT);
#endif
		/* Select ADC channel */
		switch (chan) {
			default:
			case 0: chansel = 0; break;
			case 1: chansel = 2; break;
			case 2: chansel = 1; break;
			case 3: chansel = 3; break;
		}

		gradcdac_set_adroutput(adc_hand, (0x40 | (chansel<<4)) );

		rtems_task_wake_after(1);

		/* Start single Analog to Digital conversion */
		gradcdac_adc_convert_start(adc_hand);

wait_conversion_complete:

#ifdef ADC_USE_INTERRUPT
		/* Wait for conversion to complete, the interrupt will signal
		 * semaphore.
		 */
		rtems_semaphore_obtain(adcdac_sem, RTEMS_WAIT, RTEMS_NO_TIMEOUT);

		/* Do Work ... */
		adcdac_wakeups++;
#else
		sleep(1);
#endif

		/* Get converted value */
		adc_value=0xffff;
		ret = gradcdac_adc_convert_try(adc_hand, &adc_value);
		if ( ret == 0 ) {

			/* Move old values */
			for(i=8; i>=0; i--){
				adcvals[chan][i+1] = adcvals[chan][i];
			}

			adcvals[chan][0] = adc_value;

			/* Filter the input ADC Value */
			adc_filtered_val[chan] = adc_calc(&adcvals[chan][0]);
			adc_last_val[chan] = adc_value;

		} else if ( ret < 0 ) {
			printf("ADC: Failed\n");
		} else {
			/* skip triggering a new conversion */
			goto wait_conversion_complete;
		}
		if ( ++chan >= MAX_CHANS )
			chan = 0;
	}
}

/* Print statistics gathered by ADC Task
 *
 */
void adcdac_print_stats(void)
{
	int chan;
#ifdef ADC_SHOW_INTS
	unsigned int tmp0, tmp1;
#endif

	for (chan=0; chan<MAX_CHANS; chan++) {
		printf("ADC_IN-%d:  0x%x (last: 0x%x)\n", chan, adc_filtered_val[chan], adc_last_val[chan]);
	}

#ifdef ADC_SHOW_INTS
	/* Take a snapshot of the counters to avoid different values */
	tmp0 = adc_ints;
	tmp1 = adcdac_wakeups;

	/* Print Stats: */
	printf("ADC INTS:  %d\n",tmp0);
	rtems_task_wake_after(4);
	printf("ADC/DAC WAKES: %d\n",tmp1);
#endif
}
