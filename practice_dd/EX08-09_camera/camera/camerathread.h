#ifndef CAMERATHREAD_H
#define CAMERATHREAD_H

#include <QThread>
#include <QDebug>
#include <linux/videodev2.h>
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

signals:
    void send_data(const uchar *, int, int);

protected:
    static const int CAPTURE_MAX_BUFFER = 5;
    static char *CAPTURE_DEVICE;

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
