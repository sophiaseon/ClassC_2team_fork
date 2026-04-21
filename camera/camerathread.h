#ifndef CAMERATHREAD_H
#define CAMERATHREAD_H

#include <QThread>
#include <QDebug>
#include <QByteArray>
#include <QImage>
#include <QAtomicInt>
#include <linux/videodev2.h>
#include <sys/select.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

class CameraThread : public QThread
{
    Q_OBJECT

public:
    CameraThread(QObject *parent = 0);
    void requestStop();

    // UI가 바쁠 때 0으로 세팅하여 프레임 드롭
    QAtomicInt m_uiReady;

signals:
    // RGB888 QImage를 직접 emit - 메인 스레드 변환 부하 제거
    void send_image(QImage image);

protected:
    static const int CAPTURE_MAX_BUFFER = 5;
    static char *CAPTURE_DEVICE;

    QAtomicInt m_stop;

    struct buf_info{
        int index;
        unsigned int length;
        void *start;
    };

    struct video_dev
    {
        int fd;
        int cap_width, cap_height;
        struct buf_info buff_info[CAPTURE_MAX_BUFFER];
        int numbuffer;
    } videodev;

    void vidioc_enuminput(int fd);

private:
    unsigned int frame_count;
    int frame_devisor;

    int initCapture();
    int startCapture();
    int captureFrame();
    int stopCapture();
    void closeCapture();
    void run();
    int subInitCapture();
};

#endif // CAMERATHREAD_H
