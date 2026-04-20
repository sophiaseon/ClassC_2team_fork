#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <stdlib.h>

static volatile int running = 1;

static void signal_handler(int signo)
{
    (void)signo;
    running = 0;
}

int main(void)
{
    int fd;
    int n;
    char buf[256];

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    printf("HC-SR04 monitor start (Ctrl+C to exit)\n");

    while (running) {
        fd = open("/dev/hcsr04_array", O_RDONLY);
        if (fd < 0) {
            perror("open");
            usleep(500000);
            continue;
        }

        memset(buf, 0, sizeof(buf));

        n = read(fd, buf, sizeof(buf) - 1);
        if (n < 0) {
            perror("read");
            close(fd);
            usleep(500000);
            continue;
        }

        buf[n] = '\0';

        printf("---------------\n");
        printf("%s", buf);
        fflush(stdout);

        close(fd);

        usleep(500000);
    }

    printf("\nProgram terminated.\n");

    return 0;
}
