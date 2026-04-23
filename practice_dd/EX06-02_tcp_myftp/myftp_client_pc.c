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

#define MAX_RBUF 256
#define MAX_WBUF 256

pid_t pid;

void handle_command(int sfd)
{
	char rbuf[MAX_RBUF];
	char wbuf[MAX_WBUF];
	int len;
	int exit_flag = 0;

	for(;;) {
		printf("[%d] myftp> ", pid);
		if(fgets(rbuf, MAX_RBUF, stdin)) {
			rbuf[strlen(rbuf)-1] = '\0';
			if(strcmp(rbuf, "quit") == 0) {
				sprintf(wbuf, "QUIT\r\n");
				write(sfd, wbuf, strlen(wbuf));
				len = read(sfd, rbuf, MAX_RBUF-1);
				if(len <= 0) return;
				rbuf[len] = 0;
				printf("[%d] received: %s", pid, rbuf);
				exit_flag = 1;
			}
			else if(strcmp(rbuf, "pwd") == 0) {
				sprintf(wbuf, "PWD\r\n");
				write(sfd, wbuf, strlen(wbuf));
				len = read(sfd, rbuf, MAX_RBUF-1);
				if(len <= 0) return;
				rbuf[len] = 0;
				printf("[%d] received: %s", pid, rbuf);
			}
		}

		if(exit_flag) return;
	}
}

int main(int argc, char **argv)
{
	int ret;
	int sfd;
	struct sockaddr_in addr_server;

	if(argc != 1) {
		printf("usage:\n");
		return EXIT_FAILURE;
	}
	printf("[%d] running %s\n", pid = getpid(), argv[0]);

	sfd = socket(AF_INET, SOCK_STREAM, 0);
	if(sfd == -1) {
		printf("[%d] error: %s (%d)\n", pid, strerror(errno), __LINE__);
		return EXIT_FAILURE;
	}

	memset(&addr_server, 0, sizeof(addr_server));
	addr_server.sin_family = AF_INET;
	addr_server.sin_addr.s_addr = inet_addr(SERVER_IP);
	addr_server.sin_port = htons(SERVER_PORT);

	ret = connect(sfd, (struct sockaddr *)&addr_server, sizeof(addr_server));
	if(ret == -1) {
		printf("[%d] error: %s (%d)\n", pid, strerror(errno), __LINE__);
		return EXIT_FAILURE;
	}
	printf("[%d] connected\n", pid);

	handle_command(sfd);

	close(sfd);
	printf("[%d] closed\n", pid);

	printf("[%d] terminated\n", pid);

	return EXIT_SUCCESS;
}

