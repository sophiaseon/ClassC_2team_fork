#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>

#define MAX_RAND_NUM 4

int main(void)
{
	int i, fd;
	char key[MAX_RAND_NUM];

	fd = open("/dev/urandom", O_RDONLY);
	if(fd == -1) {
		printf("randtest: %s (%d)\n", strerror(errno), __LINE__);
		return EXIT_FAILURE;
	}

	if(read(fd, key, MAX_RAND_NUM) == -1) {
		printf("randtest: %s (%d)\n", strerror(errno), __LINE__);
		return EXIT_FAILURE;
	}

	close(fd);

	for(i=0; i<MAX_RAND_NUM; i++) {
		printf("%02X\n", key[i]);
	}

	return EXIT_SUCCESS;
}
