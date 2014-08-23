
/* RTEMS sample driver for memscrub.
 * See memscrub_rtems.h for more information.
 *
 * Author: Magnus Hjorth, Aeroflex Gaisler
 * Contact: support@gaisler.com
 */



#include <stdio.h>
#include <stdlib.h>
#include <drvmgr/drvmgr.h>
#include <drvmgr/ambapp_bus.h>
#include "memscrub_rtems.h"
#include "memscrub.h"

struct memscrubr_priv {
  int irq;
  unsigned long memstart, memsize;
  int autostart;
  int state;
  int scrubdelay,regendelay;
  int regenthres;
  int opermode;
  rtems_id msgq;
  struct memscrub ms;
  struct ddr2sp_regs *dr;
};


/* Interrupt service routine */
static void memscrubr_isr(int irq, void *arg)
{
  struct memscrubr_priv *priv = (struct memscrubr_priv *)arg;
  unsigned long m[2];
  int evmask,j;
  unsigned long ahberr_addr, ahberr_data;
  int runcount;

  while (1) {
    /* Get events and clear interrupt status in core */
    evmask = memscrub_get_events(&(priv->ms),&ahberr_addr,&ahberr_data,&runcount);
    if (evmask == 0) break;

    /* Based on events, format messages and send to user via message queue */
    if ((evmask & (MEMSCRUB_UE|MEMSCRUB_CE)) != 0) {
      m[0] = evmask | (ahberr_data << 16);
      m[1] = ahberr_addr;
      j = rtems_message_queue_send(priv->msgq,m,8);
      if (j!=RTEMS_SUCCESSFUL && j!=RTEMS_TOO_MANY) 
	rtems_fatal_error_occurred(j);
    }
    if ((evmask & MEMSCRUB_DONE) != 0) {
      m[0] = evmask;
      m[1] = runcount;

      j = rtems_message_queue_send(priv->msgq,m,8);
      if (j!=RTEMS_SUCCESSFUL && j!=RTEMS_TOO_MANY) 
	rtems_fatal_error_occurred(j);    
    }
    
    /* Perform actions */
    if ((evmask & MEMSCRUB_DONE) != 0) {
      
      /* Scrub or regeneration run done - restart scrub mode*/
      priv->state = 1;      
      if (priv->opermode == 1) 
	j=MEMSCRUB_FLAG_IRQD; 
      else {
	j=MEMSCRUB_FLAG_LOOP;
	memscrub_setup_scruberr(&(priv->ms),-1,priv->regenthres);
      }
      memscrub_scrub(&(priv->ms),priv->memstart,priv->memsize,priv->scrubdelay,j);
      
    } else if (priv->state==1 && (evmask & (MEMSCRUB_CE|MEMSCRUB_CE_RUNTOT)) != 0 &&
	       memscrub_get_runcount(&(priv->ms)) >= priv->regenthres) {
      
      /* Switch to regeneration mode */
      priv->state = 2;
      memscrub_setup_scruberr(&(priv->ms),-1,-1);
      memscrub_switch_mode(&(priv->ms),MEMSCRUB_MODE_REGEN,
			   priv->regendelay,MEMSCRUB_FLAG_IRQD);
      

      m[0] = 32;
      j = rtems_message_queue_send(priv->msgq,m,4);
      if (j!=RTEMS_SUCCESSFUL && j!=RTEMS_TOO_MANY)
	rtems_fatal_error_occurred(j);
    }
  }
}

static int set_option_main(struct memscrubr_priv *priv, char *optname, int value)
{
  if (priv == NULL) return -1;
  if (!strcmp(optname,"memstart")) {
    priv->memstart = (unsigned long)value;
  } else if (!strcmp(optname,"memsize")) {
    if (value <= 0) return -3;
    priv->memsize = (unsigned long)value;
  } else if (!strcmp(optname,"autostart")) {
    if (value != 0 && value != 1) return -3;
    priv->autostart = value;
  } else if (!strcmp(optname,"opermode")) {
    if (value != 0 && value != 1) return -3;
    priv->opermode = value;
  } else if (!strcmp(optname,"regenthres")) {
    if (value < 0) return -3;
    priv->regenthres = value;
  } else if (!strcmp(optname,"scrubdelay")) {
    if (value < 0) return -3;
    priv->scrubdelay = value;
  } else if (!strcmp(optname,"regendelay")) {
    if (value < 0) return -3;
    priv->regendelay = value;
  } else {
    return -2;
  }
  return 0;
}

/* Init1: Initialize hardware and data structures */
int memscrubr_init1(struct rtems_drvmgr_dev_info *dev)
{
  int i,j;
  struct amba_dev_info *adi = (struct amba_dev_info *)dev->businfo;
  struct memscrubr_priv *priv = (struct memscrubr_priv *)dev->priv;
  unsigned long regaddr;
  struct rtems_drvmgr_key *keys;

  /* Default options */
  memset(priv,0,sizeof(*priv));
  priv->regenthres = 5;
  priv->scrubdelay = 100;
  priv->regendelay = 10;

  /* Set options from resources associated with device */
  keys = NULL;
  i = rtems_drvmgr_keys_get(dev,&keys);
  if (i != 0) return -1;
  
  for (i=0; keys[i].key_name!=NULL; i++) {
    j = set_option_main(priv, keys[i].key_name, keys[i].key_value.i);
    if (j != 0) return -1;
  }
  
  /* Find scrubber register address */
  for (i=0; i<4; i++)
    if (adi->info.ahb_slv->type[i]==AMBA_TYPE_AHBIO) {
      regaddr = adi->info.ahb_slv->start[i];
      break;
    }
  if (i >= 4) return -1;

  /* Init memscrub device */
  memscrub_init(regaddr, &(priv->ms));

  return 0;
}


static int memscrubr_start_main(struct memscrubr_priv *priv);


/* Init2: Register ISR, enable interrupts and autostart (if enabled) */
int memscrubr_init2(struct rtems_drvmgr_dev_info *dev)
{
  struct memscrubr_priv *priv = (struct memscrubr_priv *)dev->priv;
  int i,j;

  j = 1;
  /* Create message queue for ISR */
  i = rtems_message_queue_create(rtems_build_name('S','C','R','B'),8,8,RTEMS_GLOBAL,&(priv->msgq));

  /* Register and enable ISR */
  if (i == 0)
    i = j = rtems_drvmgr_interrupt_register(dev,0,memscrubr_isr,priv);  
  if (i == 0) 
    i = rtems_drvmgr_interrupt_enable(dev,0,memscrubr_isr,priv);

  /* Enable error interrupts for CE and UE */
  if (i == 0 && !priv->autostart)
    memscrub_setup_ahberr(&(priv->ms),0,0);

  /* Autostart scrubber */
  if (i==0 && priv->autostart) {
    i = memscrubr_start_main(priv);    
  }

  /* Error handling */
  if (i != 0 && j == 0)
    rtems_drvmgr_interrupt_unregister(dev,0,memscrubr_isr,priv);    
  
  if (i != 0 && priv->autostart)
    rtems_fatal_error_occurred(-1);

  return i;
}

static int memscrubr_remove(struct rtems_drvmgr_dev_info *dev)
{
  struct memscrubr_priv *priv = (struct memscrubr_priv *)dev->priv;
  memscrub_disable(&(priv->ms));
  rtems_drvmgr_interrupt_unregister(dev,0,memscrubr_isr,priv);
  return 0;
}

static int memscrubr_info(struct rtems_drvmgr_dev_info *dev, int a, int b)
{
  return 0;
}

static struct rtems_drvmgr_drv_ops memscrubr_ops =
{
  .init = { memscrubr_init1, memscrubr_init2 },
  .remove = memscrubr_remove,
  .info = memscrubr_info
};

static struct amba_dev_id memscrubr_ids[] = {
  {VENDOR_GAISLER, GAISLER_MEMSCRUB},
  {0, 0}
};

static struct amba_drv_info memscrubr_drv_info =
{
	{
		NULL,				/* Next driver */
		NULL,				/* Device list */
		DRIVER_AMBAPP_GAISLER_MEMSCRUB_ID,/* Driver ID */
		"MEMSCRUB_DRV",			/* Driver Name */
		DRVMGR_BUS_TYPE_AMBAPP,		/* Bus Type */
		&memscrubr_ops,
		0,				/* No devices yet */
		sizeof(struct memscrubr_priv)   /* Priv size */
	},
	&memscrubr_ids[0]
};

void memscrubr_register(void)
{
  static int run_before = 0;
  if (run_before) return;
  run_before++;
  rtems_drvmgr_drv_register(&memscrubr_drv_info.general);
}

static struct memscrubr_priv *getpriv(int index, struct rtems_drvmgr_dev_info **rdev)
{
  struct rtems_drvmgr_dev_info *dev;
  dev = memscrubr_drv_info.general.dev;
  while (index > 0 && dev != NULL) {
    dev = dev->next_in_drv;
    index--;
  }
  if (dev == NULL) return NULL; 
  if (rdev != NULL) *rdev=dev;
  return (struct memscrubr_priv *)dev->priv;
}

int memscrubr_count(void)
{
  struct rtems_drvmgr_dev_info *dev = memscrubr_drv_info.general.dev;
  int i = 0;
  while (dev != NULL) { i++; dev=dev->next_in_drv; }
  return i;
}

static int memscrubr_start_main(struct memscrubr_priv *priv)
{
  int f,ct,rt;
  if (priv == NULL) return -1;
  memscrub_stop(&(priv->ms));
  if (priv->opermode == 0) {
    f = MEMSCRUB_FLAG_LOOP;
    ct = -1;
    rt = priv->regenthres;
  } else {
    f = MEMSCRUB_FLAG_IRQD;
    ct = 0;
    rt = -1;
  }  
  priv->state = 1;
  memscrub_setup_ahberr(&(priv->ms),0,ct);
  memscrub_setup_scruberr(&(priv->ms),-1,rt);
  memscrub_scrub(&(priv->ms),priv->memstart,priv->memsize,priv->scrubdelay,f);
  return 0;
}

int memscrubr_start(int index)
{
  return memscrubr_start_main(getpriv(index,NULL));
}

int memscrubr_stop(int index)
{
  struct memscrubr_priv *priv;
  priv = getpriv(index,NULL);
  if (priv == NULL) return -1;
  memscrub_stop(&(priv->ms));
  priv->state = 0;
  return 0;  
}

int memscrubr_print_status(int index)
{
  struct memscrubr_priv *priv;
  priv = getpriv(index,NULL);
  if (priv == NULL) return -1;
  printf("Scrubber dev %p, regs @ %p, mem @ %08x-%08x, state %d, autostart=%d\n",
	 (void *)priv,(void *)priv->ms.regs,(unsigned)priv->memstart,
	 (unsigned)(priv->memstart+priv->memsize-1),priv->state,priv->autostart);
  return 0;
}

int memscrubr_get_message(int index, int block, struct memscrubr_message *msgout)
{
  struct memscrubr_priv *priv;
  int i;
  size_t s;
  unsigned long m[2];

  msgout->msgtype = 0;
  priv = getpriv(index,NULL);
  if (priv == NULL) return -1;

  i = rtems_message_queue_receive(priv->msgq, m, &s, (block?RTEMS_WAIT:RTEMS_NO_WAIT), RTEMS_NO_TIMEOUT);
  if (i == RTEMS_UNSATISFIED && !block) return 0;
  if (i != RTEMS_SUCCESSFUL) return -1;

  if ((m[0] & 15) != 0) {
    msgout->msgtype = 2;
    msgout->d.err.errtype = (m[0] & 1) ? 1 : 2;
    msgout->d.err.addr = m[1];
    msgout->d.err.hsize = (m[0] >> 16) & 7;
    msgout->d.err.master = (m[0] >> 19) & 15;
    msgout->d.err.hwrite = (m[0] >> 23) & 1;
  } else if ((m[0] & 16) != 0) {
    msgout->msgtype = 1;
    msgout->d.done.cecount = m[1];
  } else if ((m[0] & 32) != 0) {
    msgout->msgtype = 3;
  }

  return 0;
}

int memscrubr_set_option(int index, char *resname, int value)
{
  return set_option_main(getpriv(index,NULL),resname,value);
}

int memscrubr_get_totals(int index, int totals[2])
{
  struct memscrubr_priv *priv;
  struct rtems_drvmgr_dev_info *dev;
  int i;

  priv = getpriv(index,&dev);
  if (priv == NULL) return -1;

  i = rtems_drvmgr_interrupt_disable(dev,0,memscrubr_isr,priv);
  if (i != 0) return i;  

  memscrub_get_totals(&(priv->ms),totals);

  i = rtems_drvmgr_interrupt_enable(dev,0,memscrubr_isr,priv);
  if (i != 0) rtems_fatal_error_occurred(1);

  return i;
}
