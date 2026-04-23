#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>

#define DEV_NAME "/dev/mydev"

int main(int argc, char *argv[])
{
	int fd;

	fd = open(DEV_NAME, O_RDWR);
	if(fd == -1) {
                printf("apptest: %s (%d)\n", strerror(errno), __LINE__);
                return EXIT_FAILURE;
	}
	printf("apptest: %s opened\n", DEV_NAME);
	sleep(1); // to avoid mixing messages

	close(fd);
	printf("apptest: %s closed\n", DEV_NAME);

	return EXIT_SUCCESS;
}
