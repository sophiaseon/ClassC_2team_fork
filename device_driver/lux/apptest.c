#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

int main() {
    int fd;
    unsigned short lux_value;
    int ret;

    // 1. 드라이버 오픈 (이때 커널 드라이버의 bh1750_open이 실행되어 센서가 깨어납니다)
    printf("BH1750 장치를 여는 중...\n");
    fd = open("/dev/bh1750", O_RDONLY);
    
    if (fd < 0) {
        fprintf(stderr, "오류: /dev/bh1750을 열 수 없습니다. (%s)\n", strerror(errno));
        printf("팁: sudo insmod bh1750.ko를 했는지, 권한이 있는지(sudo 사용) 확인하세요.\n");
        return -1;
    }

    printf("장치 오픈 성공! 2초간 1초 간격으로 조도를 측정합니다.\n\n");
    printf("  측정 횟수  |   조도(Lux)   \n");
    printf("----------------------------\n");

    for (int i = 0; i < 10; i++) {
        // 2. 데이터 읽기 (드라이버가 계산한 unsigned short 값을 직접 가져옴)
        ret = read(fd, &lux_value, sizeof(lux_value));
        
        if (ret < 0) {
            perror("데이터 읽기 실패");
            break;
        } else if (ret == sizeof(lux_value)) {
            printf("     %2d      |     %u lx\n", i + 1, lux_value);
        }

        // 센서가 다음 데이터를 갱신할 시간을 줌
        sleep(1);
    }

    // 3. 장치 닫기 (이때 커널 드라이버의 bh1750_release가 실행되어 센서가 절전 모드로 들어갑니다)
    printf("----------------------------\n");
    printf("테스트 종료. 장치를 닫고 센서를 절전 모드로 전환합니다.\n");
    close(fd);

    return 0;
}

