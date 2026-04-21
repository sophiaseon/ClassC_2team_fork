#ifndef ALARMCAMERATHREAD_H
#define ALARMCAMERATHREAD_H

#include <QAtomicInt>
#include <QImage>
#include <QMutex>
#include <QThread>

#include <linux/videodev2.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>

class AlarmCameraThread : public QThread
{
    Q_OBJECT

public:
    explicit AlarmCameraThread(QObject *parent = nullptr);
    ~AlarmCameraThread() override;

    void stop();
    void requestCapture(const QString &savePath);
    void setLuxThreshold(float lux);

    // Set to 1 after UI finishes displaying a frame (slot should call this)
    QAtomicInt m_uiReady;

signals:
    void frameReady(const QImage &frame);
    void captureSaved(const QString &path, bool ok, const QString &errorText);
    void cameraError(const QString &errorText);
    void captureRejectedLowLight(float lux, float threshold);
    void statusUpdate(const QString &msg);

protected:
    void run() override;

private:
    static const int CAPTURE_MAX_BUFFER = 4;

    struct BufInfo {
        int index;
        unsigned int length;
        void *start;
    };

    struct VideoDev {
        int fd;
        int cap_width;
        int cap_height;
        BufInfo buff_info[CAPTURE_MAX_BUFFER];
        int numbuffer;
    } m_videodev;

    int  initCapture();
    int  startCapture();
    int  captureFrame();
    int  stopCapture();
    void closeCapture();

    // BH1750
    bool  initBH1750();
    float readLux();
    void  closeBH1750();

    bool m_running;
    QMutex m_captureLock;
    QString m_pendingCapturePath;
    qint64 m_lastEmitMs;

    int   m_i2cFd;
    float m_luxThreshold;
};

#endif // ALARMCAMERATHREAD_H
