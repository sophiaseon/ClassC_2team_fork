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
#include <stdarg.h>
#include <ctype.h>

#include "define.h"

#define MAX_RBUF 512
#define MAX_WBUF 512
#define MAX_TBUF 256
#define MAX_FBUF 256

#define BUF_SIZE 1024

pid_t pid;

int new_client(uint32_t srv_addr, unsigned short port)
{
	int client;
	struct sockaddr_in addr;
	int ret;

	client = socket(AF_INET, SOCK_STREAM, 0);
	if(client == -1) {
		printf("[%d] error: %s (%d)\n", pid, strerror(errno), __LINE__);
		exit(EXIT_FAILURE);
	}

	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(srv_addr);
	addr.sin_port = htons(port);

	ret = connect(client, (struct sockaddr *)&addr, sizeof(addr));
	if(ret == -1) {
		printf("[%d] error: %s (%d)\n", pid, strerror(errno), __LINE__);
		return -1;
	}

	return client;
}

int recv_file(int peer, FILE *f)
{
	char filebuf[BUF_SIZE+1];
	int len = 0;
	size_t uret;

	for(;;) {
		len = read(peer, filebuf, BUF_SIZE);
		if(len == -1) {
			printf("[%d] error: %s (%d)\n", pid, strerror(errno), __LINE__);
			return -1;
		}
		else if(len == 0) {
			break;
		}

		uret = fwrite(filebuf, 1, len, f);
		if(uret < len) {
			printf("[%d] error: %s (%d)\n", pid, strerror(errno), __LINE__);
			return -1;
		}
	}

	return 0;
}

int recv_path(int peer, char *file)
{
	int ret;
	FILE *f = fopen(file, "w");
	if(f == NULL) {
		printf("error: %s (%d)\n", strerror(errno), __LINE__);
		return -1;
	}

	ret = recv_file(peer, f);

	fclose(f);

	return ret;
}

void handle_command(int sfd)
{
	char rbuf[MAX_RBUF];
	char wbuf[MAX_WBUF];
	char tbuf[MAX_TBUF];
	char fbuf[MAX_FBUF];
	int len;
	int exit_flag = 0;
	unsigned short port_num;

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
			else if(strcmp(rbuf, "ls") == 0) {
				unsigned int addr = ntohl(inet_addr(SERVER_IP));
				int data_client;
				sprintf(wbuf, "LS\r\n");
				write(sfd, wbuf, strlen(wbuf));
				len = read(sfd, rbuf, MAX_RBUF-1);
				if(len <= 0) return;
				rbuf[len] = 0;
				printf("[%d] received: %s", pid, rbuf);
				if(strcmp(rbuf, "ERR\r\n") == 0) continue;

				sscanf(rbuf, "%s %hu", tbuf, &port_num);
				data_client = new_client(addr, port_num);
				if(data_client < 0) {
					printf("[%d] error: %d (%d)\n", pid, data_client, __LINE__);
					return;
				}

				recv_file(data_client, stdout);
				len = read(sfd, rbuf, MAX_RBUF-1);
				if(len <= 0) return;
				rbuf[len] = 0;
				printf("[%d] received: %s", pid, rbuf);
			}
			else if(strncmp(rbuf, "get ", 4) == 0) {
				unsigned int addr = ntohl(inet_addr(SERVER_IP));
				int data_client;
				sscanf(rbuf, "%s %s", tbuf, fbuf);
				sprintf(wbuf, "GET %s\r\n", fbuf);
				write(sfd, wbuf, strlen(wbuf));
				len = read(sfd, rbuf, MAX_RBUF-1);
				if(len <= 0) return;
				rbuf[len] = 0;
				printf("[%d] received: %s", pid, rbuf);
				if(strcmp(rbuf, "ERR\r\n") == 0) continue;

				sscanf(rbuf, "%s %hu", tbuf, &port_num);
				data_client = new_client(addr, port_num);
				if(data_client < 0) {
					printf("[%d] error: %d (%d)\n", pid, data_client, __LINE__);
					return;
				}

				recv_path(data_client, fbuf);
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

