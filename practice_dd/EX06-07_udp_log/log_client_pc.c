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

int get_date(char *str)
{
	FILE * fp_r;
	size_t ulen;
	int ret;

	fp_r = popen("date +'%y-%m-%d %H:%M:%S'", "r");
	if(fp_r == NULL) {
		printf("[%d] error: %s (%d)\n", pid, strerror(errno), __LINE__);
		return -1;
	}
	ulen = fread(str, 1, DATE_SIZE-1, fp_r);
	if(ulen <= 0) {
		printf("[%d] error: %s (%d)\n", pid, strerror(errno), __LINE__);
		return -1;
	} 
	str[ulen-1] = '\0'; // to remove '\n' at the end

	ret = pclose(fp_r);
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
	socklen_t addr_server_len;
	char buf[MAX_BUF];
	info_t info;

	if(argc != 2) {
		printf("usage: %s {msg}\n", argv[0]);
		return EXIT_FAILURE;
	}
	printf("[%d] running %s %s\n", pid = getpid(), argv[0], argv[1]);

	ret = get_date(info.date);
	if(ret == -1) {
		printf("[%d] error: %s (%d)\n", pid, "can't get date", __LINE__);
		return EXIT_FAILURE;
	}

	strcpy(info.msg, argv[1]);

	sfd = socket(AF_INET, SOCK_DGRAM, 0);
	if(sfd == -1) {
		printf("[%d] error: %s (%d)\n", pid, strerror(errno), __LINE__);
		return EXIT_FAILURE;
	}

	memset(&addr_client, 0, sizeof(addr_client));
	addr_client.sin_family = AF_INET;
	addr_client.sin_addr.s_addr = htonl(INADDR_ANY);
	addr_client.sin_port = 0; /* A random number is assigned */ 
	ret = bind(sfd, (struct sockaddr *)&addr_client, sizeof(addr_client));
	if(ret == -1) {
		printf("[%d] error: %s (%d)\n", pid, strerror(errno), __LINE__);
		return EXIT_FAILURE;
	}

	memset(&addr_server, 0, sizeof(addr_server));
	addr_server.sin_family = AF_INET;
	addr_server.sin_addr.s_addr = inet_addr(SERVER_IP);
	addr_server.sin_port = htons(SERVER_PORT);
	sendto(sfd, &info, sizeof(info), 0, (struct sockaddr *)&addr_server, sizeof(addr_server));

	addr_server_len = sizeof(addr_server);
	len = recvfrom(sfd, buf, MAX_BUF-1, 0, (struct sockaddr *)&addr_server, &addr_server_len);
	if(len > 0) {
		buf[len] = 0;
		printf("[%d] received: %s\n", pid, buf);
	}

	close(sfd);
	printf("[%d] closed\n", pid);

	printf("[%d] terminated\n", pid);

	return EXIT_SUCCESS;
}

