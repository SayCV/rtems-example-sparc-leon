
#include <stdio.h>
#include <stdlib.h>

#define OCCAN_SPEED_1000K 1000000
#define OCCAN_SPEED_800K  800000
#define OCCAN_SPEED_500K  500000
#define OCCAN_SPEED_250K  250000
#define OCCAN_SPEED_125K  125000
#define OCCAN_SPEED_75K   75000
#define OCCAN_SPEED_50K   50000
#define OCCAN_SPEED_25K   25000
#define OCCAN_SPEED_10K   10000


/*  register bit definitions */
#define OCCAN_BUSTIM_SJW       0xc0
#define OCCAN_BUSTIM_BRP       0x3f
#define OCCAN_BUSTIM_SJW_BIT   6

#define OCCAN_BUSTIM_SAM       0x80
#define OCCAN_BUSTIM_TSEG2     0x70
#define OCCAN_BUSTIM_TSEG2_BIT 4
#define OCCAN_BUSTIM_TSEG1     0x0f

#define MAX_TSEG1 15
#define MAX_TSEG2 7

typedef struct {
	unsigned char btr0;
	unsigned char btr1;
} occan_speed_regs;

struct grcan_timing {
	unsigned char scaler;
	unsigned char ps1;
	unsigned char ps2;
	unsigned int  rsj;
	unsigned char bpr;
};

/* This function calculates BTR0 BTR1 values for a given bitrate.
 * Heavily based on mgt_mscan_bitrate() from peak driver, which 
 * in turn is based on work by Arnaud Westenberg.
 *
 * Set communication parameters.
 * baud rate in Hz
 * input clock frequency of can core in Hz (system frequency)
 * sjw synchronization jump width (0-3) prescaled clock cycles
 * sampl_pt sample point in % (0-100) sets (TSEG1+2)/(TSEG1+TSEG2+3)
 *                                                               ratio
 */
int occan_calc_speedregs(unsigned int clock_hz, unsigned int rate, occan_speed_regs *result){
	int best_error = 1000000000;
	int error;
	int best_tseg=0, best_brp=0, best_rate=0, brp=0;
	int tseg=0, tseg1=0, tseg2=0;
	int sjw = 0;
	int clock = clock_hz / 2;
	int sampl_pt = 80;

	if ( (rate<5000) || (rate>1000000) ){
		/* invalid speed mode */
		return -1;
	}
	
	/* find best match, return -2 if no good reg 
	 * combination is available for this frequency */

	/* some heuristic specials */
	if (rate > ((1000000 + 500000) / 2))
		sampl_pt = 75;

	if (rate < ((12500 + 10000) / 2))
		sampl_pt = 75;

	if (rate < ((100000 + 125000) / 2))
		sjw = 1;

	/* tseg even = round down, odd = round up */
	for (tseg = (0 + 0 + 2) * 2;
	     tseg <= (MAX_TSEG2 + MAX_TSEG1 + 2) * 2 + 1;
	     tseg++)
	{
		brp = clock / ((1 + tseg / 2) * rate) + tseg % 2;
		if ((brp == 0) || (brp > 64))
			continue;

		error = rate - clock / (brp * (1 + tseg / 2));
		if (error < 0)
		{
			error = -error;
		}

		if (error <= best_error)
		{
			best_error = error;
			best_tseg = tseg/2;
			best_brp = brp-1;
			best_rate = clock/(brp*(1+tseg/2));
		}
	}

	if (best_error && (rate / best_error < 10))
	{
		return -2;
	}else if ( !result )
		return 0; /* nothing to store result in, but a valid bitrate can be calculated */

	tseg2 = best_tseg - (sampl_pt * (best_tseg + 1)) / 100;

	if (tseg2 < 0)
	{
		tseg2 = 0;
	}

	if (tseg2 > MAX_TSEG2)
	{
		tseg2 = MAX_TSEG2;
	}

	tseg1 = best_tseg - tseg2 - 2;

	if (tseg1 > MAX_TSEG1)
	{
		tseg1 = MAX_TSEG1;
		tseg2 = best_tseg - tseg1 - 2;
	}
/*
	result->sjw = sjw;
	result->brp = best_brp;
	result->tseg1 = tseg1;
	result->tseg2 = tseg2;
*/
	result->btr0 = (sjw<<OCCAN_BUSTIM_SJW_BIT) | (best_brp&OCCAN_BUSTIM_BRP);
	result->btr1 = (0<<7) | (tseg2<<OCCAN_BUSTIM_TSEG2_BIT) | tseg1;
	
	return 0;
}

#undef MIN_TSEG1
#undef MIN_TSEG2
#undef MAX_TSEG1
#undef MAX_TSEG2

#define MIN_TSEG1 1
#define MIN_TSEG2 2
#define MAX_TSEG1 14
#define MAX_TSEG2 8

int grcan_calc_timing(
  unsigned int baud,          /* The requested BAUD to calculate timing for */
  unsigned int core_hz,       /* Frequency in Hz of GRCAN Core */
  unsigned int sampl_pt,
  struct grcan_timing *timing /* result is placed here */
  )
{
	int best_error = 1000000000;
	int error;
	int best_tseg=0, best_brp=0, best_rate=0, brp=0;
	int tseg=0, tseg1=0, tseg2=0;
	int sjw = 1;
  
  /* Default to 90% */
  if ( (sampl_pt < 50) || (sampl_pt>99) ){
    sampl_pt = 90;
  }

	if ( (baud<5000) || (baud>1000000) ){
		/* invalid speed mode */
		return -1;
	}
	
	/* find best match, return -2 if no good reg 
	 * combination is available for this frequency
   */

	/* some heuristic specials */
	if (baud > ((1000000 + 500000) / 2))
		sampl_pt = 75;

	if (baud < ((12500 + 10000) / 2))
		sampl_pt = 75;

	/* tseg even = round down, odd = round up */
	for (tseg = (MIN_TSEG1 + MIN_TSEG2 + 2) * 2;
	     tseg <= (MAX_TSEG2 + MAX_TSEG1 + 2) * 2 + 1;
	     tseg++)
	{
		brp = core_hz / ((1 + tseg / 2) * baud) + tseg % 2;
		if ((brp <= 0) || 
        ( (brp > 256*1) && (brp <= 256*2) && (brp&0x1) ) ||
        ( (brp > 256*2) && (brp <= 256*4) && (brp&0x3) ) ||
        ( (brp > 256*4) && (brp <= 256*8) && (brp&0x7) ) ||
        (brp > 256*8)
        )
			continue;

		error = baud - core_hz / (brp * (1 + tseg / 2));
		if (error < 0)
		{
			error = -error;
		}

		if (error <= best_error)
		{
			best_error = error;
			best_tseg = tseg/2;
			best_brp = brp-1;
			best_rate = core_hz/(brp*(1+tseg/2));
		}
	}

	if (best_error && (baud / best_error < 10))
	{
		return -2;
	}else if ( !timing )
		return 0; /* nothing to store result in, but a valid bitrate can be calculated */

	tseg2 = best_tseg - (sampl_pt * (best_tseg + 1)) / 100;

	if (tseg2 < MIN_TSEG2)
	{
		tseg2 = MIN_TSEG2;
	}

	if (tseg2 > MAX_TSEG2)
	{
		tseg2 = MAX_TSEG2;
	}

	tseg1 = best_tseg - tseg2 - 2;

	if (tseg1 > MAX_TSEG1)
	{
		tseg1 = MAX_TSEG1;
		tseg2 = best_tseg - tseg1 - 2;
	}
  
  /* Get scaler and BRP from pseudo BRP */
  if ( best_brp <= 256 ){
    timing->scaler = best_brp;
    timing->bpr = 0;
  }else if ( best_brp <= 256*2 ){
    timing->scaler = ((best_brp+1)>>1) -1;
    timing->bpr = 1;
  }else if ( best_brp <= 256*4 ){
    timing->scaler = ((best_brp+1)>>2) -1;
    timing->bpr = 2;
  }else{
    timing->scaler = ((best_brp+1)>>3) -1;
    timing->bpr = 3;
  }
  
	timing->ps1    = tseg1+1;
	timing->ps2    = tseg2;
	timing->rsj    = sjw;

	return 0;
}

unsigned int steplist[] = 
{
  OCCAN_SPEED_1000K,
  OCCAN_SPEED_800K,
  OCCAN_SPEED_500K,
  OCCAN_SPEED_250K,
  OCCAN_SPEED_125K,
  OCCAN_SPEED_75K,
  OCCAN_SPEED_50K,
  OCCAN_SPEED_25K,
  OCCAN_SPEED_10K,
  0
};

void print_timing(unsigned int corefreq, unsigned int rate, occan_speed_regs *timing)
{
  unsigned int baud;
  unsigned int sjw, brp, tseg1, tseg2;
  double err;
  
  printf("BTR0: 0x%x\n",timing->btr0);
  printf("BTR1: 0x%x\n",timing->btr1);
  
  
  tseg1 = timing->btr1 & OCCAN_BUSTIM_TSEG1;
  tseg2 = (timing->btr1>>OCCAN_BUSTIM_TSEG2_BIT) & 0x7;
  brp   = timing->btr0 & OCCAN_BUSTIM_BRP;
  sjw   = timing->btr0 >> OCCAN_BUSTIM_SJW_BIT;
  printf("tseg1: %d, tseg2: %d, brp: %d, sjw: %d\n",tseg1,tseg2,brp,sjw);
  
  baud = corefreq / 2;
  baud = baud / (brp+1);
  baud = baud / (3+tseg1+tseg2);
  
  printf("Baud:   %d\n",baud);
  printf("Sampl:  %d%%\n",(100*(1+tseg1))/(1+tseg1+tseg2));
  err = baud;
  err = err/rate;
  err = err*100-100; /* to % */
  printf("Err:    %f%%\n",err);
}

/* 60000000/(scaler+1)/(2^bpr)/(ps1+ps2+2) */

void print_grtiming(unsigned int corefreq, unsigned int rate, struct grcan_timing *timing)
{
  unsigned int baud;
  double err;
  printf("SCALER: 0x%x\n",timing->scaler);
  printf("PS1:    0x%x\n",timing->ps1);
  printf("PS2:    0x%x\n",timing->ps2);
  printf("RSJ:    0x%x\n",timing->rsj);
  printf("BPR:    0x%x\n",timing->bpr);
  
  baud = corefreq/(timing->scaler+1);
  baud = baud/(1<<timing->bpr);
  baud = baud/(2+timing->ps1+timing->ps2);
  printf("Baud:   %d\n",baud);
  printf("Sampl:  %d%%\n",(100*(1+timing->ps1))/(2+timing->ps1+timing->ps2));
  err = baud;
  err = err/rate;
  err = err*100-100; /* to % */
  printf("Err:    %f%%\n",err);
}

int main(int argc, char *argv[]){
  unsigned int clock_hz=40000000;
  int i,ret;
  occan_speed_regs timing;
  struct grcan_timing grtiming;
  
  if ( argc > 2 ){
    printf("usage: %s [can_core_clock_in_Hz]\n",argv[0]);
    return 0;
  }
  if ( argc == 2 ){
    clock_hz = strtoul(argv[1],NULL,10);
  }
  
  printf("Calculating OC-CAN baud rates for %dkHz\n",clock_hz/1000);
  
  printf("\n");
  i=0;
  while( steplist[i] != 0 ){
    ret = occan_calc_speedregs(clock_hz,steplist[i],&timing);
    if ( !ret ){
      printf("---------- %d bits/s ----------\n",steplist[i]);
      print_timing(clock_hz,steplist[i],&timing);
    }else{
      printf("Error getting OCCAN Baud rate for %d @ %dkHz\n",steplist[i],clock_hz/1000);
    }
    i++;
  }

  printf("\n\n\nCalculating GRCAN baud rates for %dkHz\n",clock_hz/1000);
  
  printf("\n");
  
  i=0;
  while( steplist[i] != 0 ){
    ret = grcan_calc_timing(steplist[i],clock_hz,80,&grtiming);
    if ( !ret ){
      printf("---------- %d bits/s ----------\n",steplist[i]);
      print_grtiming(clock_hz,steplist[i],&grtiming);
    }else{
      printf("Error getting GRCAN Baud rate for %d @ %dkHz\n",steplist[i],clock_hz/1000);
    }
    i++;
  }

  
  return 0;
}
