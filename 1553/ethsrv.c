#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netdb.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sched.h>

#include "ethsrv.h"
#include "config_bm.h"

/* Get LOG entries from Compressed LOG */
extern int log_cmp_take(struct bm_cmp_log *log, unsigned int *words, int max);

unsigned int debug_log[50000];
unsigned int debug_index = 0;

void debug_add(unsigned int word)
{
	if ( debug_index >= 50000 )
		debug_index = 0;
	debug_log[debug_index++] = word;
}

int cmd_get_log(int s, struct cmd_get_log *arg)
{
	static struct cmd_resp_get_log resp;
	int length;

	/* Only one device supported */
	if ( arg->devno != 0 ) {
		return -1;
	}

	/* Prepare Response */
	resp.hdr.cmdno = arg->hdr.cmdno;
	resp.devno = arg->devno;
	resp.status = 0;
	resp.log_cnt = log_cmp_take(&cmp_log, &resp.log[0], 250);

	debug_add(resp.log_cnt);

	length = offsetof(struct cmd_resp_get_log, log) +
		resp.log_cnt*sizeof(unsigned int);
	resp.hdr.length = length - sizeof(struct cmd_hdr);

	/* Send back result */
	if ( write(s, &resp, length) != length ) {
		return -1;
	}

	debug_add(length);

	return 0;
}

/* */
int ssock = -1, sock = -1;

int server_init(char *host, int port)
{
	int status;
	char name[255];
	struct sockaddr_in addr;
	struct hostent *he;
	int optval;

	ssock = socket(AF_INET, SOCK_STREAM, 0);
	if ( ssock < 0 ) {
		printf("ERROR CREATING SERVER SOCKET: %d, %d (%s)\n", ssock, errno, strerror(errno));
		return -1;
	}

	optval = 1;
	if ( setsockopt(ssock, SOL_SOCKET, SO_REUSEADDR, &optval, 4) < 0 ) {
		printf("setsockopt: %d (%s)\n", errno, strerror(errno));
	}

	memset(&addr, 0, sizeof(addr));
	
	if ( host ) {
		addr.sin_family = AF_INET;
		addr.sin_addr.s_addr = inet_addr(host);
	} else {
		gethostname(name, 255);
		he = gethostbyname(name);
		if ( he == NULL )
			return -2;
		addr.sin_family = he->h_addrtype;
	}
	addr.sin_port = htons(port);

	status = bind(ssock, (struct sockaddr *)&addr, sizeof(struct sockaddr_in));
	if ( status < 0 ) {
		close(ssock);
		ssock = -1;
		return -3;
	}

	status = listen(ssock, 2);
	if ( status < 0 ) {
		close(ssock);
		ssock = -1;
		return -4;
	}

	return 0;
}

int server_wait_client(void)
{
	/* Block waiting for new clients */
	sock = accept(ssock, NULL, NULL);
	if ( sock < 0 ) {
		printf("Failed to accept incomming client\n");
		close(ssock);
		ssock = -1;
		return -1;
	}
	return 0;
}

/* TCP/IP server loop, answers client's requests. It executes
 * until an error is detected or until the client disconnect.
 */
int server_loop(void)
{
	int err;
	int len;
	static unsigned int buf[256];
	struct cmd_hdr *hdr;

	err = 0;
	while ( err == 0 ) {

		/* Let other task have cpu between every request */
		sched_yield();

		len = read(sock, buf, sizeof(struct cmd_hdr));
		if ( len <= 0 ) {
			break;
		}

		hdr = (struct cmd_hdr *)&buf[0];
		if ( (hdr->cmdno == 0)  || (hdr->cmdno > MAX_COMMAND_NUM) ) {
			printf("Invalid command number\n");
			break;
		}

		if ( hdr->length > MAX_COMMAND_SIZE ) {
			printf("Invalid length of command: %d (MAX: %d) C:%d\n",
				hdr->length, MAX_COMMAND_SIZE, hdr->cmdno);
			break;
		}

		if ( hdr->length > 0 ) {
			/* This is not fail safe, should loop around read() until
			 * a complete command has been read.
			 */
			len = read(sock, (void *)(hdr+1), hdr->length);
			if ( len < 0 ) {
				break;
			} else if ( len != hdr->length ) {
				printf("Invalid read command length\n");
				break;
			}
		}

		/* Command handling */
		switch ( hdr->cmdno ) {

			case CMD_STATUS:
			{
				/* Not implemented */
				err = 1;
				break;
			}

			case CMD_GET_INFO:
			{
				/* Not implmented */
				err = 1;
				break;
			}

			case CMD_GET_LOG:
			{
				err = cmd_get_log(sock, (struct cmd_get_log *)hdr);
				break;
			}

			default:
				err = 1;
				break;
		}
	}

	close(sock);
	sock = -1;

	return 0;
}

void server_stop()
{
	if ( ssock >= 0 )
		close(ssock);
	if ( sock >= 0 )
		close(sock);
}
