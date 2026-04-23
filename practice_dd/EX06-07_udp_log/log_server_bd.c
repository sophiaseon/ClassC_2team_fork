#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "define.h"

#define MAX_BUF 256

pid_t pid;

int write_log(char *str, char *filename)
{
	FILE *fp_w;
	int ret;
	char cmd[64];

	sprintf(cmd, "cat >> %s", filename);
	fp_w = popen(cmd, "w");
	if(fp_w == NULL) {
		printf("[%d] error: %s (%d)\n", pid, strerror(errno), __LINE__);
		return -1;
	}
	fwrite(str, 1, strlen(str), fp_w);

	ret = pclose(fp_w);
	if(ret == -1) {
		printf("[%d] error: %s (%d)\n", pid, strerror(errno), __LINE__);
		return -1;
	}

	return 0;
}

int main(int argc, char **argv)
{
	int ret;
	int len;
	int sfd;
	struct sockaddr_in addr_server;
	struct sockaddr_in addr_client;
	socklen_t addr_client_len;
	char buf[MAX_BUF];
	info_t * p_info = (info_t *)buf;
	char tbuf[MAX_BUF];

	if(argc != 1) {
		printf("usage: %s\n", argv[0]);
		return EXIT_FAILURE;
	}
	printf("[%d] running %s\n", pid = getpid(), argv[0]);

	/* Implement code: call socket() */


	/* Implement code: call bind() */


	for(;;) {
		/* Implement code: call recvfrom(), write_log(), sendto() */

	}

	/* Implement code: call close() */


	printf("[%d] terminated\n", pid);

	return EXIT_SUCCESS;
}

