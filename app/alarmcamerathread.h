#ifndef ALARMCAMERATHREAD_H
#define ALARMCAMERATHREAD_H

#include <QImage>
#include <QMutex>
#include <QThread>

class AlarmCameraThread : public QThread
{
    Q_OBJECT

public:
    explicit AlarmCameraThread(QObject *parent = nullptr);
    ~AlarmCameraThread() override;

    void stop();
    void requestCapture(const QString &savePath);

signals:
    void frameReady(const QImage &frame);
    void captureSaved(const QString &path, bool ok, const QString &errorText);
    void cameraError(const QString &errorText);

protected:
    void run() override;

private:
    static const int kMaxBuffer = 5;

    struct BufferInfo {
        unsigned int length = 0;
        void *start = nullptr;
    };

    int initCapture();
    int configureFormat();
    int startCapture();
    int captureFrame();
    int stopCapture();
    void closeCapture();

    void yuyvToRgb(const unsigned char *yuyv, int width, int height, unsigned char *rgb);

    int m_fd;
    int m_width;
    int m_height;
    BufferInfo m_buffers[kMaxBuffer];
    int m_bufferCount;

    bool m_running;
    QMutex m_captureLock;
    QString m_pendingCapturePath;
};

#endif // ALARMCAMERATHREAD_H
