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

#define LISTEN_BACKLOG 5
#define MAX_RBUF 256
#define MAX_WBUF 256
#define MAX_TBUF 256

pid_t pid;

int get_pwd(char *buf)
{
	/* Implement code */


}

void process_command(int sfd_client)
{
	char rbuf[MAX_RBUF];
	char wbuf[MAX_WBUF];
	char tbuf[MAX_TBUF];
	int len;

	for(;;) {
		/* Implement code: call read() */


		printf("[%d] received: %s", pid, rbuf);

		if(strcmp(rbuf, "QUIT\r\n") == 0) {
			/* Implement code: call write() */


			return;
		}
		else if(strcmp(rbuf, "PWD\r\n") == 0) {
			/* Implement code: call write() */


		}
	}
}

int main(int argc, char **argv)
{
	int ret;
	int sfd_server, sfd_client;
	struct sockaddr_in addr_server;
	struct sockaddr_in addr_client;
	socklen_t addr_client_len;
	int optval = 1;

	if(argc != 1) {
		printf("usage: %s\n", argv[0]);
		return EXIT_FAILURE;
	}
	printf("[%d] running %s\n", pid = getpid(), argv[0]);

	/* Implement code: call socket() */


	/* to prevent "Address already in use" error */
	ret = setsockopt(sfd_server, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
	if(ret == -1) {
		printf("[%d] error: %s (%d)\n", pid, strerror(errno), __LINE__);
		return EXIT_FAILURE;
	}

	/* Implement code: call bind() */


	/* Implement code: call listen() */


	for(;;) {
		printf("[%d] waiting for client ...\n", pid);

		/* Implement code: call accept() */


		printf("[%d] connected\n", pid);

		process_command(sfd_client);

		/* Implement code: call close() */


		printf("[%d] closed\n", pid);
	}

	close(sfd_server);

	printf("[%d] terminated\n", pid);

	return EXIT_SUCCESS;
}

