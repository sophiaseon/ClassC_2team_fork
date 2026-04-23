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

#define LISTEN_BACKLOG 5
#define MAX_RBUF 512
#define MAX_WBUF 512
#define MAX_TBUF 256
#define MAX_FBUF 256

#define BUF_SIZE 1024

typedef void (*sighandler_t)(int);
pid_t pid;

int new_server(unsigned int inaddr, unsigned short port, int backlog)
{
	int server;
	struct sockaddr_in addr;
	int ret;
	int optval = 1;

	server = socket(AF_INET, SOCK_STREAM, 0);
	if(server == -1) {
		printf("[%d] error: %s (%d)\n", pid, strerror(errno), __LINE__);
		exit(EXIT_FAILURE);
	}

	/* to prevent "Address already in use" error */
	ret = setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
	if(ret == -1) {
		printf("[%d] error: %s (%d)\n", pid, strerror(errno), __LINE__);
		return EXIT_FAILURE;
	}

	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(inaddr);
	addr.sin_port = htons(port);
	ret = bind(server, (struct sockaddr *)&addr, sizeof(addr));
	if(ret == -1) {
		printf("[%d] error: %s (%d)\n", pid, strerror(errno), __LINE__);
		return -1;
	}

	ret = listen(server, backlog);
	if(ret == -1) {
		printf("[%d] error: %s (%d)\n", pid, strerror(errno), __LINE__);
		return -1;
	}

	return server;
}

int send_file(int peer, FILE *f)
{
	/* Implement code */

	char filebuf[BUF_SIZE+1];
	int eof_flag = 0;

	for(;;) {
		size_t uret = fread(filebuf, 1, BUF_SIZE, f);
		if(uret < (size_t)BUF_SIZE) {
			if(feof(f) != 0) {
				eof_flag = 1;
			}
			else {
				printf("[%d] error: %s (%d)\n", pid, "fread failed", __LINE__);
				return -1;
			}
		}	

		write(peer, filebuf, uret);
		if(eof_flag) break;
	}

	return 0;
}

int send_path(int peer, char *file)
{
	/* Implement code */

	int ret;
	FILE *f = fopen(file, "r");
	if(f == NULL) {
		printf("error: %s (%d)\n", strerror(errno), __LINE__);
		return -1;
	}

	ret = send_file(peer, f);

	fclose(f);

	return ret;
}

int get_pwd(char *buf)
{
	FILE *fp_r;
	int ret;
	size_t ulen;

	fp_r = popen("pwd", "r");
	if(fp_r == NULL) {
		printf("[%d] error: %s (%d)\n", pid, strerror(errno), __LINE__);
		return -1;
	}
	ulen = fread(buf, 1, MAX_RBUF-1, fp_r);
	if(ulen == 0) {
		printf("[%d] error: %s (%d)\n", pid, strerror(errno), __LINE__);
		return -1;
	}
	buf[ulen - 1] = '\0';

	ret = pclose(fp_r);
	if(ret == -1) {
		printf("[%d] error: %s (%d)\n", pid, strerror(errno), __LINE__);
		return -1;
	}

	return 0;
}

void process_command(int sfd_client)
{
	char rbuf[MAX_RBUF];
	char wbuf[MAX_WBUF];
	char tbuf[MAX_TBUF];
	char fbuf[MAX_FBUF];
	int len;
	int exit_flag = 0;

	for(;;) {
		len = read(sfd_client, rbuf, MAX_RBUF);
		if(len <= 0) return;
		rbuf[len] = 0;
		printf("[%d] received: %s", pid, rbuf);

		if(strcmp(rbuf, "QUIT\r\n") == 0) {
			sprintf(wbuf, "OK\r\n");
			write(sfd_client, wbuf, strlen(wbuf));
			exit_flag = 1;
			return;
		}
		else if(strcmp(rbuf, "PWD\r\n") == 0) {
			if(get_pwd(tbuf) == -1) {
				sprintf(wbuf, "ERR\r\n");
			}
			else {
				sprintf(wbuf, "OK %s\r\n", tbuf);
			}
			write(sfd_client, wbuf, strlen(wbuf));
		}
		else if(strcmp(rbuf, "LS\r\n") == 0) {
			int retry = 100;
			unsigned short data_port;
			int data_server;
			int data_client = -1;
			struct sockaddr_in data_client_addr;
			socklen_t data_client_len = sizeof(data_client_addr);
			FILE *p1;

			while (retry--) {
				data_port = (rand() % 64512 + 1024);
				data_server = new_server(INADDR_ANY, data_port, 1);
				if (data_server >= 0) break;
			}
			if (data_server < 0) {
				sprintf(wbuf, "ERR\r\n");
				write(sfd_client, wbuf, strlen(wbuf));
				continue;
			} else {
				sprintf(wbuf, "OK %u\r\n", data_port);
				write(sfd_client, wbuf, strlen(wbuf));
			}

			data_client = accept(data_server, (struct sockaddr *)&data_client_addr, &data_client_len);
			if(data_client == -1) {
				close(data_server);
				printf("[%d] error: %s (%d)\n", pid, strerror(errno), __LINE__);
				return;
			}

			p1 = popen("ls -l", "r");
			send_file(data_client, p1);
			close(data_client);
			pclose(p1);

			sprintf(wbuf, "OK\r\n");
			write(sfd_client, wbuf, strlen(wbuf));

			close(data_server);
		}
		else if(strncmp(rbuf, "GET ", 4) == 0) {
			/* Implement code */

			int retry = 100;
			unsigned short data_port;
			int data_server;
			int data_client = -1;
			struct sockaddr_in data_client_addr;
			socklen_t data_client_len = sizeof(data_client_addr);

			sscanf(rbuf, "%s %s", tbuf, fbuf);

			while (retry--) {
				data_port = (rand() % 64512 + 1024);
				data_server = new_server(INADDR_ANY, data_port, 1);
				if (data_server >= 0) break;
			}
			if (data_server < 0) {
				sprintf(wbuf, "ERR\r\n");
				write(sfd_client, wbuf, strlen(wbuf));
				continue;
			} else {
				sprintf(wbuf, "OK %u\r\n", data_port);
				write(sfd_client, wbuf, strlen(wbuf));
			}

			data_client = accept(data_server, (struct sockaddr *)&data_client_addr, &data_client_len);
			if(data_client == -1) {
				close(data_server);
				printf("[%d] error: %s (%d)\n", pid, strerror(errno), __LINE__);
				return;
			}

			send_path(data_client, fbuf);
			close(data_client);

			sprintf(wbuf, "OK\r\n");
			write(sfd_client, wbuf, strlen(wbuf));

			close(data_server);
		}

		if(exit_flag) return;
	}
}

int main(int argc, char **argv)
{
	int ret;
	int sfd_server, sfd_client;
	struct sockaddr_in addr_server;
	struct sockaddr_in addr_client;
	socklen_t addr_client_len;
	pid_t pid_temp;
	sighandler_t sig_ret;
	int optval = 1;

	if(argc != 1) {
		printf("usage: %s\n", argv[0]);
		return EXIT_FAILURE;
	}
	printf("[%d] running %s\n", pid = getpid(), argv[0]);

	/* to prevent child process from becoming zombie */
	sig_ret = signal(SIGCHLD, SIG_IGN);
	if(sig_ret == SIG_ERR) {
		printf("[%d] error: %s (%d)\n", pid, strerror(errno), __LINE__);
		return EXIT_FAILURE;
	}

	sfd_server = socket(AF_INET, SOCK_STREAM, 0);
	if(sfd_server == -1) {
		printf("[%d] error: %s (%d)\n", pid, strerror(errno), __LINE__);
		return EXIT_FAILURE;
	}

	/* to prevent "Address already in use" error */
	ret = setsockopt(sfd_server, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
	if(ret == -1) {
		printf("[%d] error: %s (%d)\n", pid, strerror(errno), __LINE__);
		return EXIT_FAILURE;
	}

	memset(&addr_server, 0, sizeof(addr_server));
	addr_server.sin_family = AF_INET;
	addr_server.sin_addr.s_addr = htonl(INADDR_ANY);
	addr_server.sin_port = htons(SERVER_PORT);
	ret = bind(sfd_server, (struct sockaddr *)&addr_server, sizeof(addr_server));
	if(ret == -1) {
		printf("[%d] error: %s (%d)\n", pid, strerror(errno), __LINE__);
		return EXIT_FAILURE;
	}

	ret = listen(sfd_server, LISTEN_BACKLOG);
	if(ret == -1) {
		printf("[%d] error: %s (%d)\n", pid, strerror(errno), __LINE__);
		return EXIT_FAILURE;
	}

	for(;;) {
		printf("[%d] waiting for client ...\n", pid);
		addr_client_len = sizeof(addr_client);
		sfd_client = accept(sfd_server, (struct sockaddr *)&addr_client, &addr_client_len);
		if(sfd_client == -1) {
			printf("[%d] error: %s (%d)\n", pid, strerror(errno), __LINE__);
			return EXIT_FAILURE;
		}
		printf("[%d] connected\n", pid);

		pid_temp = fork();

		if(pid_temp == -1) {
			printf("[%d] error: %s (%d)\n", pid, strerror(errno), __LINE__);
			return EXIT_FAILURE;
		}
		else if(pid_temp == 0) {
			pid = getpid();

			close(sfd_server);

			/* to prevent pclose() from returning -1 */
			sig_ret = signal(SIGCHLD, SIG_DFL);
			if(sig_ret == SIG_ERR) {
				printf("[%d] error: %s (%d)\n", pid, strerror(errno), __LINE__);
				return EXIT_FAILURE;
			}

			process_command(sfd_client);

			close(sfd_client);
			printf("[%d] closed\n", pid);

			return EXIT_SUCCESS;
		}
		else {
			close(sfd_client);
		}
	}

	close(sfd_server);

	printf("[%d] terminated\n", pid);

	return EXIT_SUCCESS;
}

