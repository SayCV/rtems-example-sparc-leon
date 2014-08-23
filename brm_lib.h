
#ifndef __BRM_LIB_H__
#define __BRM_LIB_H__

#include <bsp.h>
#include <rtems/libio.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <sched.h>
#include <ctype.h>
#include <rtems/bspIo.h>
#include <b1553brm.h>

typedef struct {
	int fd;
	int mode; /* defaults to RT */
	int txblk;
	int rxblk;
	int broadcast;
} brm_s;

typedef brm_s *brm_t;

/* 
 * return file descriptor 
 */
brm_t brmlib_open(char *devname);

void brmlib_close(brm_t chan);


int brmlib_rt_send_multiple(brm_t chan, struct rt_msg *msgs, int msgcnt);

int brmlib_rt_send(brm_t chan, struct rt_msg *msg);

int brmlib_rt_recv_multiple(brm_t chan, struct rt_msg *msgs, int msgcnt);

int brmlib_rt_recv(brm_t chan, struct rt_msg *msg);

int brmlib_bm_recv_multiple(brm_t chan, struct bm_msg *msgs, int msgcnt);

int brmlib_bm_recv(brm_t chan, struct bm_msg *msg);

/* start execute a command list */
int brmlib_bc_dolist(brm_t chan, struct bc_msg *msgs);

/* wait for command list the finish the execution */
int brmlib_bc_dolist_wait(brm_t chan);


/* mode = 0,1,2
 * 0 = BC
 * 1 = RT
 * 2 = BM
 */
int brmlib_set_mode(brm_t chan, unsigned int mode);

/* bus=0,1,2,3
 * 0 = none
 * 1 = bus A
 * 2 = bus B
 * 3 = bus A and B 
 */
int brmlib_set_bus(brm_t chan, unsigned int bus);

int brmlib_set_txblock(brm_t chan, int txblocking);
int brmlib_set_rxblock(brm_t chan, int rxblocking);
int brmlib_set_block(brm_t chan, int txblocking, int rxblocking);
int brmlib_set_broadcast(brm_t chan, int broadcast);
int brmlib_set_std(brm_t chan, int std);
int brmlib_set_rt_addr(brm_t chan, unsigned int address);
int brmlib_set_msg_timeout(brm_t chan, unsigned int timeout);

/* DEBUG FUNCTIONS */
void print_rt_msg(int i, struct rt_msg *msg);
void print_bm_msg(int i, struct bm_msg *msg);

#endif
