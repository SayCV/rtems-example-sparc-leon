#include <rtems.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>

#include <b1553brm.h>
#include "brm_lib.h"

/* The rtems name to errno table
 *
rtems_assoc_t errno_assoc[] = {
    { "OK",                 RTEMS_SUCCESSFUL,                0 },
    { "BUSY",               RTEMS_RESOURCE_IN_USE,           EBUSY },
    { "INVALID NAME",       RTEMS_INVALID_NAME,              EINVAL },
    { "NOT IMPLEMENTED",    RTEMS_NOT_IMPLEMENTED,           ENOSYS },
    { "TIMEOUT",            RTEMS_TIMEOUT,                   ETIMEDOUT },
    { "NO MEMORY",          RTEMS_NO_MEMORY,                 ENOMEM },
    { "NO DEVICE",          RTEMS_UNSATISFIED,               ENODEV },
    { "INVALID NUMBER",     RTEMS_INVALID_NUMBER,            EBADF},
    { "NOT RESOURCE OWNER", RTEMS_NOT_OWNER_OF_RESOURCE,     EPERM},
    { "IO ERROR",           RTEMS_IO_ERROR,                  EIO},
    { 0, 0, 0 },
};
*/

brm_t brmlib_open(char *devname){
	int fd;
	brm_t ret = NULL;

	printf("brmlib_open: Opening driver %s\n", devname);
	
	fd = open(devname, O_RDWR);
	if ( fd >= 0 ){
		printf("brmlib_open: allocating memory %d\n",sizeof(*ret));
		ret = calloc(sizeof(*ret),1);
		ret->fd = fd;
		/* Initial state of driver */
		ret->mode = BRM_MODE_RT;
	}else{
		if ( errno == ENODEV ){
			printf("brmlib_open: %s doesn't exist\n", devname);
		}else if ( errno == EBUSY ){
			printf("brmlib_open: %s already taken\n", devname);
		}else{
			printf("brmlib_open: errno: %d, ret: %d\n",errno,fd);
		}
	}
	
	return ret;
}

void brmlib_close(brm_t chan){
	if ( !chan || (chan->fd<0) )
		return;
	close(chan->fd);
	free(chan);
}

int brmlib_rt_send_multiple(brm_t chan, struct rt_msg *msgs, int msgcnt){
	int ret, len, cnt;
	if ( !chan || !msgs || (msgcnt<0) )
		return -1;
	
	if ( msgcnt == 0 )
		return 0;
	
	ret = write(chan->fd,msgs,msgcnt);
	if ( ret < 0 ){
		/* something went wrong 
		 * OR in non-blocking mode
		 * that would block
		 */
		if ( !chan->txblk && (errno == EBUSY) ){
			/* would block ==> 0 sent is ok */
			return 0;
		}
			
		if ( errno == EINVAL ){
			/* CAN must be started before receiving */
			printf("brmlib_rt_send_multiple: input descriptor numbering error\n");
			return -1;
		}

		printf("brmlib_send_multiple: error in write, errno: %d, returned: %d\n",errno,ret);
		return -1;
	}
	
	/* sent all of them */
	return ret;
}

int brmlib_rt_send(brm_t chan, struct rt_msg *msg){
	return brmlib_rt_send_multiple(chan,msg,1);
}

int brmlib_recv_multiple(brm_t chan, void *msgs, int msglen){
	int ret, len, cnt;
	
	if ( !chan || !msgs || (msglen<0) )
		return -1;
	
	if ( msglen == 0 )
		return 0;
	
	errno = 0;
	ret = read(chan->fd,msgs,msglen);
	if ( ret < 0 ){
		/* something went wrong 
		 * OR in non-blocking mode
		 * that would block
		 */
		if ( !chan->rxblk && (errno == EBUSY) ){
			return 0;
		}
		
		printf("brmlib_recv_multiple: error in read, errno: %d, returned: %d\n",errno,ret);
		return -1;
	}
	
	/* message count is returned, not byte count */
	return ret;
}

int brmlib_rt_recv_multiple(brm_t chan, struct rt_msg *msgs, int msgcnt){
	int len;
	
	if ( !chan || (chan->mode!=BRM_MODE_RT) )
		return -1;
	
	/* Read the messages */
	return brmlib_recv_multiple(chan,(void *)msgs,msgcnt);
}

int brmlib_bm_recv_multiple(brm_t chan, struct bm_msg *msgs, int msgcnt){
	int len;
	
	if ( !chan || (chan->mode!=BRM_MODE_BM) )
		return -1;
	
	/* Read the messages */
	return brmlib_recv_multiple(chan,(void *)msgs,msgcnt);
}

int brmlib_rt_recv(brm_t chan, struct rt_msg *msg){
	return brmlib_rt_recv_multiple(chan,msg,1);
}

int brmlib_bm_recv(brm_t chan, struct bm_msg *msg){
	return brmlib_bm_recv_multiple(chan,msg,1);
}


int brmlib_set_mode(brm_t chan, unsigned int mode){
	int ret;
	unsigned int arg = mode;
	
	if ( !chan )
		return -1;
	
	ret = ioctl(chan->fd,BRM_SET_MODE,&arg);
	if ( ret < 0 ){
				
		if ( errno == EINVAL ){
			printf("brmlib_set_mode: invalid mode: %d\n",arg);
			return -2;
		}
	
		if ( errno == ENOMEM ){
			/* started */
			printf("brmlib_set_mode: not enough memory\n");
			return -3;
		}
		
		/* unhandeled errors */
		printf("brmlib_set_mode: failed, errno: %d, ret: %d\n",errno,ret);
		return -1;
	}
	
	/* Save mode */
	chan->mode = mode;
	
	return 0;
}

int brmlib_set_bus(brm_t chan, unsigned int bus){
	int ret;
	unsigned int arg = bus;
	if ( !chan )
		return -1;
		
	/* only for RT mode */
	if ( chan->mode != BRM_MODE_RT ){
		printf("brmlib_set_bus: Only possible to set bus in RT mode\n");
		return -2; /* fast EINVAL... */
	}
	
	ret = ioctl(chan->fd,BRM_SET_BUS,&arg);
	if ( ret < 0 ){
				
		if ( errno == EINVAL ){
			printf("brmlib_set_bus: invalid bus: %d\n",arg);
			return -2;
		}
		
		/* unhandeled errors */
		printf("brmlib_set_bus: failed, errno: %d, ret: %d\n",errno,ret);
		return -1;
	}
	return 0;
}

int brmlib_set_msg_timeout(brm_t chan, unsigned int timeout){
	int ret;
	unsigned int arg = timeout;
	
	if ( !chan )
		return -1;
	
	if ( !((chan->mode==BRM_MODE_BM) || (chan->mode==BRM_MODE_BC)) ){
		printf("brmlib_set_msg_timeout: Only possible to set bus in BC & BM mode\n");
		return -2;
	}
	
	ret = ioctl(chan->fd,BRM_SET_MSGTO,&arg);
	if ( ret < 0 ){
		if ( errno == EBUSY ){
			/* started */
			printf("brmlib_set_msg_timeout: started\n");
			return -2;
		}
		
		printf("brmlib_set_msg_timeout: failed, errno: %d, ret: %d\n",errno,ret);
		return -1;
	}
	
	return 0;
}

int brmlib_set_rt_addr(brm_t chan, unsigned int address){
	int ret;
	unsigned int arg = address;
	
	if ( !chan )
		return -1;
	
	if ( chan->mode != BRM_MODE_RT ){
		printf("brmlib_set_rt_addr: not in RT mode\n");
		return -2;
	}
	
	ret = ioctl(chan->fd,BRM_SET_RT_ADDR,&arg);
	if ( ret < 0 ){
		printf("brmlib_set_rt_addr: failed, errno: %d, ret: %d\n",errno,ret);
		return -1;
	}
	
	return 0;
}

int brmlib_set_std(brm_t chan, int std){
	int ret;
	unsigned int arg = std;
	
	if ( !chan )
		return -1;
	
	ret = ioctl(chan->fd,BRM_SET_STD,&arg);
	if ( ret < 0 ){
	
		if ( errno == EINVAL ){
			/* started */
			printf("brmlib_set_std: new standard not valid: %d\n",arg);
			return -2;
		}
		
		printf("brmlib_set_filter: failed, errno: %d, ret: %d\n",errno,ret);
		return -1;
	}
	
	return 0;
}


int brmlib_set_txblock(brm_t chan, int txblocking){
	unsigned int arg = (txblocking) ? 1 : 0;
	int ret;
	
	if ( !chan )
		return -1;
	
	ret = ioctl(chan->fd,BRM_TX_BLOCK,&arg);
	if ( ret < 0 ){
		printf("brmlib_set_txblock: failed, errno: %d, ret: %d\n",errno,ret);
		return -1;
	}
	/* remember blocking state */
	chan->txblk = arg;
	return 0;
}


int brmlib_set_rxblock(brm_t chan, int rxblocking){
	unsigned int arg = (rxblocking) ? 1 : 0;
	int ret;
	
	if ( !chan )
		return -1;
	
	ret = ioctl(chan->fd,BRM_RX_BLOCK,&arg);
	if ( ret < 0 ){
		printf("brmlib_set_rxblock: failed, errno: %d, ret: %d\n",errno,ret);
		return -1;
	}
	/* remember blocking state */
	chan->rxblk = arg;
	return 0;
}

int brmlib_set_block(brm_t chan, int txblocking, int rxblocking){
	int ret;
	ret = brmlib_set_txblock(chan,txblocking);
	if ( !ret ){
		return brmlib_set_rxblock(chan,rxblocking);
	}
	return ret;
}

int brmlib_set_broadcast(brm_t chan, int broadcast){
	unsigned int arg = (broadcast) ? 1 : 0;
	int ret;
	
	if ( !chan )
		return -1;
	
	ret = ioctl(chan->fd,BRM_SET_BCE,&arg);
	if ( ret < 0 ){
		printf("brmlib_set_broadcast: failed, errno: %d, ret: %d\n",errno,ret);
		return -1;
	}
	
	/* remember broadcast state */
	chan->broadcast = arg;
	return 0;
}

int brmlib_bc_dolist(brm_t chan, struct bc_msg *msgs){
	int ret;
	
	ret = ioctl(chan->fd, BRM_DO_LIST, msgs);
	if ( ret < 0 ){
		if ( errno == EINVAL ){
			printf("brmlib_bc_dolist: not in BC mode\n");
			return -2;
		}

		if ( errno == EBUSY ){
			printf("brmlib_bc_dolist: busy\n");
			return -3;
		}
		
		printf("brmlib_bc_dolist: errno %d, ret: %d\n",errno,ret);
		return -1;
	}
	return 0;
}

int brmlib_bc_dolist_wait(brm_t chan){
	int ret;
	unsigned int result;
	
	ret = ioctl(chan->fd, BRM_LIST_DONE, &result);
	if ( ret < 0 ){
		if ( errno == EINVAL ){
			printf("brmlib_bc_dolist: not in BC mode\n");
			return -2;
		}

		if ( errno == EBUSY ){
			printf("brmlib_bc_dolist: busy\n");
			return -3;
		}
		
		printf("brmlib_bc_dolist: errno %d, ret: %d\n",errno,ret);
		return -1;
	}
	
	return result;
}

void print_rt_msg(int i, struct rt_msg *msg){
	int k, wc;
	wc = msg->miw >> 11;
	printf("MSG[%d]: miw: 0x%x, time: 0x%x, desc: 0x%x, len: %d\n  ",i,msg->miw,msg->time,msg->desc,wc);
	/* print data */			
	for (k = 0; k < wc; k++){
		if ( isalnum(msg->data[k]) ){
  		printf("0x%x (%c)",msg->data[k],msg->data[k]);
		}else{
			printf("0x%x (.)",msg->data[k]);
		}
	}
	if ( k > 0 )
		printf("\n");
}

void print_bm_msg(int i, struct bm_msg *msg){
	int k,wc,tr,desc;
	wc = msg->cw1 & 0x1f;	
	desc = (msg->cw1 >> 5) & 0x1f;
	tr = msg->cw1 & 0x0400;

	printf("MSG[%d]: miw: 0x%x, cw1: 0x%x, cw2: 0x%x, desc: %d\n",i,msg->miw,msg->cw1,msg->cw2,desc);
	printf("         sw1: 0x%x, sw2: 0x%x, tim: 0x%x, len: %d\n",msg->sw1,msg->sw2,msg->time,wc);
	
	/* no message data in BC transmit commands */
	if ( tr )
		return;
	
	printf("         ");
	
	/* print data */			
	for (k = 0; k<wc; k++){
		if ( isalnum(msg->data[k]) ){
  		printf("0x%x (%c)",msg->data[k],msg->data[k]);
		}else{
			printf("0x%x (.)",msg->data[k]);
		}
	}
	if ( k > 0 )
		printf("\n");
}
