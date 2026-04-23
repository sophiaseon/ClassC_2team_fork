#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <sys/ioctl.h>
#include "my_ioctl.h"

#define DEV_NAME "/dev/mybuzzer"

int main(int argc, char *argv[])
{
	int fd, len;
	char *wbuf;
	int wlen;

	fd = open(DEV_NAME, O_RDWR);
	if(fd == -1) {
		printf("apptest: %s (%d)\n", strerror(errno), __LINE__);
		return EXIT_FAILURE;
	}
	printf("apptest: %s opened\n", DEV_NAME);
	sleep(1); // to avoid mixing messages

	if(argc == 3) {
		if(strcmp(argv[1], "W") == 0) {
			wbuf = argv[2];
			len = strlen(wbuf);

			wlen = write(fd, wbuf, len);
			if(wlen == -1) {
				printf("apptest: %s (%d)\n", strerror(errno), __LINE__);
				return EXIT_FAILURE;
			}
			wbuf[wlen] = 0;
			printf("apptest: %d bytes written [%s]\n", wlen, wbuf);
			sleep(1); // to avoid mixing messages
		}
	}

	close(fd);
	printf("apptest: %s closed\n", DEV_NAME);

	return EXIT_SUCCESS;
}

