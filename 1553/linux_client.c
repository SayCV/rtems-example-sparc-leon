#include <stdlib.h>
#if !(defined WIN32 || defined __MINGW32__ )
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#else
#include <windows.h>
#include <winsock.h>
#endif
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include "ethsrv.h"
#include "config_bm.h"

#if (defined WIN32 || defined __MINGW32__)
static int win32_initsocket = 0;
void sys_sockerror (char *s) {
  int err = WSAGetLastError();
  if (err == 10054) return;
  else if(err = 10044) {
    printf("You might not have TCP/IP turned on\n");
  }
}

void winInitSocket() { 
  if (!win32_initsocket) {
    short version = MAKEWORD(2,0);
    WSADATA nobby;
    if (WSAStartup(version,&nobby)) sys_sockerror("WSAstartup");
    win32_initsocket = 1;
  }
}
#endif

int client_connect(char *host, int port)
{
	int sock;
	struct sockaddr_in addr;
	struct hostent *he;

	memset(&addr, 0, sizeof(addr));

#if defined WIN32 || defined __MINGW32__
	winInitSocket();
#endif

	if ( atoi(host) ) {
		/* Server name given as IP-number */
		addr.sin_family = AF_INET;
		addr.sin_addr.s_addr = inet_addr(host);
	} else {
		/* Server name given as Hostname */
		he = gethostbyname(host);
		if ( he == NULL )
			return -1;

		addr.sin_family = he->h_addrtype;
	}
	addr.sin_port = htons(port);

	sock = socket(AF_INET, SOCK_STREAM, 0);
	if ( sock < 0 ) {
		return -2;
	}

	if ( connect(sock, (const struct sockaddr *)&addr, sizeof(addr)) < 0 ) {
		close(sock);
		return -3;
	}

	return sock;
}

int client_get_log(int sock, struct cmd_resp_get_log *resp)
{
	int i, len;
	struct cmd_get_log cmd;

	/* Init GET-LOG command */
	cmd.hdr.length = htons(1);
	cmd.hdr.cmdno = CMD_GET_LOG;
	cmd.devno = 0;

	/* Send Command */
	len = send(sock, (void *)&cmd, 5, 0);
	if ( len != sizeof(cmd) ) {
		printf("GOT: %d, errno: %d (%s)\n", len, errno, strerror(errno));
		return -1;
	}

	/* Wait for response */
	len = recv(sock, (void *)resp, 8, 0);
	if ( len != 8 ) {
		return -2;
	}
	resp->hdr.length = ntohs(resp->hdr.length);
	if ( resp->log_cnt > 0 ) {
		i=0;
		len = 0;
retry:
		len += recv(sock, (void *)&resp->log[i], (resp->log_cnt - i)*4, 0);
		if ( len != resp->log_cnt*4 ) {
			i = len / 4;
			goto retry;

			printf("Got %d requested %d\n", len, resp->log_cnt*4);
			return -3;
		}
	}
	/* Convert info host by order */
	for ( i=0; i<resp->log_cnt; i++) {
		resp->log[i] = ntohl(resp->log[i]);
	}

	return 0;
}

int main(int argc, char *argv[])
{
	char *tgtname, *filename;
	int sock, err, i;
	struct cmd_resp_get_log logresp;
	FILE *fp;
	char buf[4096], *bufend;
	int tot;

	tgtname = argv[1];
	filename = argv[2];

	if ( (argc != 3) || !tgtname ) {
		printf("usage: %s IPNUM_OF_RTEMS_TARGET FILENAME\n", argv[0]);
		return -1;
	}
	
	printf("Target Name: %s\n", tgtname);
	printf("File Name: %s\n", filename);

	fp = fopen(filename, "w");
	if ( fp == NULL ) {
		printf("Failed to open file\n");
		return -1;
	}

	/* Connect to server */
	sock = client_connect(tgtname, ETHSRV_PORT);
	if ( sock < 0 ) {
		printf("Failed to connect to RTEMS Target server: %d\n", sock);
		return -1;
	}

	printf("Connected to RTEMS Server, Starting logging\n");

	tot = 0;
	while ( 1 ) {
		/* Get LOG entry */
		err = client_get_log(sock, &logresp);
		if ( err ) {
			printf("### GET LOG FAILED: %d. Total: %d\n", err, tot);
			exit(-1);
		}
		
		if ( logresp.log_cnt < 1 ) {
			printf("No input available, entries read: %d\n", tot);
			fflush(NULL);
			usleep(100000); /* Sleep 100ms */
			continue;
		}

		tot += logresp.log_cnt;

		/* Convert to ascii */
		bufend = &buf[0];
		for ( i=0; i<logresp.log_cnt; i++) {
			bufend += sprintf(bufend, "%08x\n", logresp.log[i]);
		}

		/* Put LOG Entries to file */
		fwrite(buf, (unsigned int)bufend-(unsigned int)buf, 1, fp);
	}

	close(sock);
	fclose(fp);

	return 0;	
}
