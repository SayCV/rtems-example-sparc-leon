#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <string.h>
#include <ctype.h>

#include <occan.h>
#include "occan_lib.h"

/* The rtems to errno table
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

occan_t occanlib_open(char *name) {
	int fd;
	occan_t ret = NULL;

	printf("occanlib_open: Opening driver %s\n", name);

	fd = open(name, O_RDWR);
	if ( fd >= 0 ){
		printf("occanlib_open: allocating memory %d\n",sizeof(*ret));
		ret = calloc(sizeof(*ret),1);
		ret->fd = fd;
	}else{
		if ( errno == ENODEV ){
			printf("occanlib_open: channel %s doesn't exist\n", name);
		}else if ( errno == EBUSY ){
			printf("occanlib_open: channel already taken\n");
		}else if ( errno == ENOMEM ){
			printf("occanlib_open: unable to allocate default buffers\n");
		}else{
			printf("occanlib_open: errno: %d, ret: %d\n",errno,fd);
		}
	}
	
	return ret;
}

void occanlib_close(occan_t chan){
	if ( !chan )
		return;
	close(chan->fd);
	free(chan);
}

int occanlib_send_multiple(occan_t chan, CANMsg *msgs, int msgcnt){
	int ret, len, cnt;
	if ( !chan || !msgs || (msgcnt<0) )
		return -1;
	
	if ( msgcnt == 0 )
		return 0;
	
	len = sizeof(CANMsg)*msgcnt;
	
	ret = write(chan->fd,msgs,len);
	if ( ret < 0 ){
		/* something went wrong 
		 * OR in non-blocking mode
		 * that would block
		 */
		if ( !chan->txblk && (errno == ETIMEDOUT) ){
			/* would block ==> 0 sent is ok */
			return 0;
		}
		
		if ( errno == EBUSY ){
			/* CAN must be started before receiving */
			printf("occanlib_send_multiple: CAN is not started\n");
			return -2;
		}
		
		if ( errno == EINVAL ){
			/* CAN must be started before receiving */
			printf("occanlib_send_multiple: length of buffer wrong\n");
			return -1;
		}

		printf("occanlib_send_multiple: error in write, errno: %d, returned: %d\n",errno,ret);
		return -1;
	}

	if ( ret != len ){
		cnt = ret / sizeof(CANMsg);
		return cnt;
	}
	/* sent all of them */
	return msgcnt;
}

int occanlib_send(occan_t chan, CANMsg *msg){
	return occanlib_send_multiple(chan,msg,1);
}

int occanlib_recv_multiple(occan_t chan, CANMsg *msgs, int msgcnt){
	int ret, len, cnt;
	
	if ( !chan || !msgs || (msgcnt<0) )
		return -1;
	
	if ( msgcnt == 0 )
		return 0;
	
	/* calc total length in bytes */
	len = sizeof(CANMsg)*msgcnt;
	
	errno = 0;
	ret = read(chan->fd,msgs,len);
	if ( ret < 0 ){
		/* something went wrong 
		 * OR in non-blocking mode
		 * that would block
		 */
		if ( !chan->rxblk && (errno == ETIMEDOUT) ){
			return 0;
		}
		
		if ( chan->rxblk && (errno == EIO) ){
			/* BUS OFF */
			printf("occanlib_recv_multiple: BUS OFF during blocking read\n");
			return -3;
		}
		
		if ( errno == EBUSY ){
			/* CAN must be started before receiving */
			printf("occanlib_recv_multiple: CAN is not started\n");
			return -2;
		}
		
		if ( errno == EINVAL ){
			/* CAN must be started before receiving */
			printf("occanlib_recv_multiple: length of input wrong\n");
			return -1;
		}

		printf("occanlib_recv_multiple: error in read, errno: %d, returned: %d\n",errno,ret);
		return -1;
	}
	
	if ( ret != len ){
		cnt = ret / sizeof(CANMsg);
		return cnt;
	}
	/* sent all of them */
	return msgcnt;
}

int occanlib_recv(occan_t chan, CANMsg *msg){
	return occanlib_recv_multiple(chan,msg,1);
}

int occanlib_set_speed(occan_t chan, unsigned int speed){
	int ret;
	if ( !chan )
		return -1;
	
	ret = ioctl(chan->fd,OCCAN_IOC_SET_SPEED,speed);
	if ( ret < 0 ){
		
		if ( errno == EBUSY ){
			/* started */
			printf("occanlib_set_speed: started\n");
			return -2;
		}
		
		if ( errno == EINVAL ){
			printf("occanlib_set_speed: invalid speed: %d\n",speed);
			return -3;
		}
	
		printf("occanlib_set_speed: failed, errno: %d, ret: %d\n",errno,ret);
		return -1;
	}
	
	return 0;
}

int occanlib_set_speed_auto(occan_t chan){
	if ( !chan )
		return -1;
	
	printf("occanlib_set_speed_auto: not implemented\n");
	return -100;
}

int occanlib_set_btrs(occan_t chan, unsigned char btr0, unsigned char btr1){
	int ret;
	unsigned int btr0btr1;
	
	if ( !chan )
		return -1;
	
	btr0btr1 = (btr0<<8) | btr1;
	
	ret = ioctl(chan->fd,OCCAN_IOC_SET_BTRS,btr0btr1);
	if ( ret < 0 ){
		if ( errno == EBUSY ){
			/* started */
			printf("occanlib_set_btrs: started\n");
			return -2;
		}
		
		printf("occanlib_set_btrs: failed, errno: %d, ret: %d\n",errno,ret);
		return -1;
	}
	
	return 0;
}

int occanlib_set_buf_length(occan_t chan, unsigned short txlen, unsigned short rxlen){
	int ret;
	unsigned int lengths;
	
	if ( !chan )
		return -1;
	
	lengths = txlen<<16 | rxlen;
	
	ret = ioctl(chan->fd,OCCAN_IOC_SET_BUFLEN,lengths);
	if ( ret < 0 ){
		if ( errno == EBUSY ){
			/* started */
			printf("occanlib_set_buf_length: started\n");
			return -2;
		}

		if ( errno == ENOMEM ){
			/* started */
			printf("occanlib_set_buf_length: no memory for buffers: rxlen: %d, txlen: %d\n",rxlen,txlen);
			return -3;
		}
				
		printf("occanlib_set_buf_length: failed, errno: %d, ret: %d\n",errno,ret);
		return -1;
	}
	
	return 0;
}

int occanlib_get_stats(occan_t chan, occan_stats *stats){
	int ret;
	
	if ( !chan || !stats )
		return -1;
	
	ret = ioctl(chan->fd,OCCAN_IOC_GET_STATS,stats);
	if ( ret < 0 ){
		if ( errno == EINVAL ){
			printf("occanlib_get_stats: stats not valid: 0x%p\n", stats);
			return -3;
		}
		
		printf("occanlib_get_stats: failed, errno: %d, ret: %d\n",errno,ret);
		return -1;
	}
	
	return 0;
}

int occanlib_set_filter(occan_t chan, struct occan_afilter *new_filter){
	int ret;
	
	if ( !chan || !new_filter )
		return -1;
	
	ret = ioctl(chan->fd,OCCAN_IOC_SET_FILTER,new_filter);
	if ( ret < 0 ){
	
		if ( errno == EBUSY ){
			/* started */
			printf("occanlib_set_filter: started\n");
			return -2;
		}
		
		if ( errno == EINVAL ){
			/* started */
			printf("occanlib_set_filter: new_filter not valid: 0x%p\n", new_filter);
			return -3;
		}
		
		printf("occanlib_set_filter: failed, errno: %d, ret: %d\n",errno,ret);
		return -1;
	}
	
	return 0;
}

int occanlib_set_blocking_mode(occan_t chan, int txblocking, int rxblocking){
	unsigned int arg = 0;
	int ret;
	
	if ( !chan )
		return -1;
	
	if ( rxblocking )
		arg |= OCCAN_BLK_MODE_RX;
		
	if ( txblocking )
		arg |= OCCAN_BLK_MODE_TX;
	
	ret = ioctl(chan->fd,OCCAN_IOC_SET_BLK_MODE,arg);
	if ( ret < 0 ){
		printf("occanlib_set_blocking_mode: failed, errno: %d, ret: %d\n",errno,ret);
		return -1;
	}
	/* remember blocking state */
	chan->txblk = txblocking;
	chan->rxblk = rxblocking;
	return 0;
}

int occanlib_get_status(occan_t chan, unsigned int *status){
	int ret;
	
	if ( !chan || !status )
		return -1;
	
	ret = ioctl(chan->fd,OCCAN_IOC_GET_STATUS,status);
	if ( ret < 0 ){
		
		if ( errno == EINVAL ){
			printf("occanlib_get_status: status is NULL pointer\n");
			return -1;
		}
		
		printf("occanlib_get_status: failed, errno: %d, ret: %d\n",errno,ret);
		return -1;
	}
	return 0;
}

int occanlib_start(occan_t chan){
	int ret;
	
	if ( !chan )
		return -1;
	
	ret = ioctl(chan->fd,OCCAN_IOC_START,0);
	if ( ret < 0 ){
	
		if ( errno == EBUSY ){
			printf("occanlib_start: already started\n");
			return -2;		
		}
	
		if ( errno == ENOMEM ){
			printf("occanlib_start: rx/tx not properly allocated, forgot to check" \
			       "return status from failing occanlib_set_buf_length?\n");
			return -3;
		}
	
		printf("occanlib_start: failed, errno: %d, ret: %d\n",errno,ret);
		return -1;
	}
	return 0;
}

int occanlib_stop(occan_t chan){
	int ret;
	
	if ( !chan )
		return -1;
	
	ret = ioctl(chan->fd,OCCAN_IOC_STOP,0);
	if ( ret < 0 ){
	
		if ( errno == EBUSY ){
			printf("occanlib_stop: not started\n");
			return -2;		
		}
		
		printf("occanlib_stop: failed, errno: %d, ret: %d\n",errno,ret);
		return -1;
	}
	return 0;
}


void occanlib_stats_summary_print(occan_stats *stats){
	printf("Messages Received:    %d\n",stats->rx_msgs);
	printf("Messages Transmitted: %d\n",stats->tx_msgs);	
	printf("Error Warning Interrupts:    %d\n",stats->err_warn);
	printf("Data Overrun Interrupts:     %d\n",stats->err_dovr);
	printf("Error Passive Interrupts:    %d\n",stats->err_errp);
	printf("Arbitration Losses:          %d\n",stats->err_arb);
	printf("Bus error Interrupts:        %d\n",stats->err_bus);
	printf("Total number of interrupts:  %d\n",stats->ints);	
}

/* Print out */
void occanlib_stats_arblost_print(occan_stats *stats){
	int i;
	printf("Arbitration loss frequency per bit:\n");	
	for(i=0; i<32; i++){
		if ( i>=10 ) 
			printf("Arb bit%d: %d\n",i,stats->err_arb_bitnum[i]);
		else
			printf("Arb bit%d:  %d\n",i,stats->err_arb_bitnum[i]);
	}
}

void occanlib_stats_buserr_print(occan_stats *stats){
	printf("Transmission errors: %d\n",stats->err_bus_rx);
	printf("Reception errors: %d\n",stats->err_bus_tx);
	
	printf("Bit errors: %d\n",stats->err_bus_bit);
	printf("Form erros: %d\n",stats->err_bus_form);
	printf("Stuff errors: %d\n",stats->err_bus_stuff);
	printf("Other errors: %d\n",stats->err_bus_other);
	
	printf("Stats where error occured in frame:\n");
	printf("ID 28..21: %d\n",stats->err_bus_segs[OCCAN_SEG_ID28]);
	printf("ID 20..18: %d\n",stats->err_bus_segs[OCCAN_SEG_ID20]);
	printf("ID 17..13: %d\n",stats->err_bus_segs[OCCAN_SEG_ID17]);
	printf("ID 12..5:  %d\n",stats->err_bus_segs[OCCAN_SEG_ID12]);
	printf("ID 4..0:   %d\n",stats->err_bus_segs[OCCAN_SEG_ID4]);
	printf("Start of Frame: %d\n",stats->err_bus_segs[OCCAN_SEG_START]);
	printf("Bit SRTR: %d\n",stats->err_bus_segs[OCCAN_SEG_SRTR]);
	printf("Bit IDE: %d\n",stats->err_bus_segs[OCCAN_SEG_IDE]);
	printf("Bit RTR: %d\n",stats->err_bus_segs[OCCAN_SEG_RTR]);
	printf("Bit Resv0: %d\n",stats->err_bus_segs[OCCAN_SEG_RSV0]);
	printf("Bit Resv1: %d\n",stats->err_bus_segs[OCCAN_SEG_RSV1]);
	printf("Data Length: %d\n",stats->err_bus_segs[OCCAN_SEG_DLEN]);
	printf("Data Field: %d\n",stats->err_bus_segs[OCCAN_SEG_DFIELD]);
	printf("CRC Sequence: %d\n",stats->err_bus_segs[OCCAN_SEG_CRC_SEQ]);
	printf("CRC Delimiter: %d\n",stats->err_bus_segs[OCCAN_SEG_CRC_DELIM]);
	printf("Ack Slot: %d\n",stats->err_bus_segs[OCCAN_SEG_ACK_SLOT]);
	printf("Ack Delimiter: %d\n",stats->err_bus_segs[OCCAN_SEG_ACK_DELIM]);
	printf("End of Frame: %d\n",stats->err_bus_segs[OCCAN_SEG_EOF]);
	printf("Intermission: %d\n",stats->err_bus_segs[OCCAN_SEG_INTERMISSION]);
	printf("Active error flag: %d\n",stats->err_bus_segs[OCCAN_SEG_ACT_ERR]);
	printf("Passive error flag: %d\n",stats->err_bus_segs[OCCAN_SEG_PASS_ERR]);
	printf("Tolerate dominant bits: %d\n",stats->err_bus_segs[OCCAN_SEG_DOMINANT]);
	printf("Error delimiter: %d\n",stats->err_bus_segs[OCCAN_SEG_EDELIM]);
	printf("Oveload flag: %d\n",stats->err_bus_segs[OCCAN_SEG_OVERLOAD]);
}

void occanlib_stats_print(occan_stats *stats){
	/* Print all stats */
	occanlib_stats_summary_print(stats);
	occanlib_stats_buserr_print(stats);
	occanlib_stats_arblost_print(stats);
}


/* CAN HELP DEBUG FUNCTIONS */
char *msgstr_type[2] = {"STD", "EXT"};
char *msgstr_rtr[2] = {"", " RTR"};

void print_msg(int i, CANMsg *msg){
	int j;
	char data_str_buf[64];
	int ofs;
	
	if ( !msg )
		return;
	
	if ( i > 0 ){
		printf("MSG[%d]: %s%s length: %d, id: 0x%x\n",i,msgstr_type[(int)msg->extended],msgstr_rtr[(int)msg->rtr],msg->len,msg->id);
		/* print data */
		if ( msg->len > 0 ){
			ofs = sprintf(data_str_buf,"MSGDATA[%d]: ",i);
			for(j=0; j<msg->len; j++){
				ofs+=sprintf(data_str_buf+ofs,"0x%02x ",msg->data[j]);
			}
			printf("%s  ",data_str_buf);
			ofs=0;
			for(j=0; j<msg->len; j++){
				if ( isalnum(msg->data[j]) )
					ofs+=sprintf(data_str_buf+ofs,"%c",msg->data[j]);
				else
					ofs+=sprintf(data_str_buf+ofs,".");
			}
			printf("%s\n",data_str_buf);
		}
	}else{
		printf("MSG: %s%s length: %d, id: 0x%x\n",msgstr_type[(int)msg->extended],msgstr_rtr[(int)msg->rtr],msg->len,msg->id);
		/* print data */
		if ( msg->len > 0 ){
			ofs = sprintf(data_str_buf,"MSGDATA: ");
			for(j=0; j<msg->len; j++){
				ofs+=sprintf(data_str_buf+ofs,"0x%02x ",msg->data[j]);
			}
			printf("%s  ",data_str_buf);
			ofs=0;
			for(j=0; j<msg->len; j++){
				if ( isalnum(msg->data[j]) )
					ofs+=sprintf(data_str_buf+ofs,"%c",msg->data[j]);
				else
					ofs+=sprintf(data_str_buf+ofs,".");
			}
			printf("%s\n",data_str_buf);
		}
	}
}


