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
#include <sys/syscall.h>
#include <pthread.h>

#include "define.h"

#define LISTEN_BACKLOG 5
#define MAX_BUF 256

pid_t pid;

void *thread_func(void *arg)
{
	int sfd_client = *(int *)arg;
	int len;
	char buf[MAX_BUF];
	pid_t tid = syscall(__NR_gettid);

	printf("[%d] thread started\n", tid);

	for(;;) {
		len = read(sfd_client, buf, MAX_BUF-1);
		if(len <= 0) {
			close(sfd_client);
			printf("[%d] closed\n", tid);
			break;
		}
		else {
			buf[len] = 0;
			printf("[%d] received: %s\n", tid, buf);

			/* echo back */
			write(sfd_client, buf, len);
		}
	}

	pthread_exit(NULL);
}

int main(int argc, char **argv)
{
	int ret;
	int sfd_server, sfd_client;
	struct sockaddr_in addr_server;
	struct sockaddr_in addr_client;
	socklen_t addr_client_len;
	int optval = 1;
	pthread_t thread_id;

	if(argc != 1) {
		printf("usage: %s\n", argv[0]);
		return EXIT_FAILURE;
	}
	printf("[%d] running %s\n", pid = getpid(), argv[0]);

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

		printf("[%d] creating thread\n", pid);
		ret = pthread_create(&thread_id, NULL, thread_func, &sfd_client);
		if(ret != 0) {
			printf("[%d] error: %d (%d)\n", pid, ret, __LINE__);
			return EXIT_FAILURE;
		}
		pthread_detach(thread_id);
	}

	close(sfd_server);

	printf("[%d] terminated\n", pid);

	return EXIT_SUCCESS;
}

