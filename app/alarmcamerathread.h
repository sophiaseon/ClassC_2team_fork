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

    // BH1750 조도 임계값 설정 (기본값: 50 lux)
    void setLuxThreshold(float lux);

signals:
    void frameReady(const QImage &frame);
    void captureSaved(const QString &path, bool ok, const QString &errorText);
    void cameraError(const QString &errorText);
    // 조도 부족으로 촬영 거부 시 발생 (현재 lux, 임계값)
    void captureRejectedLowLight(float lux, float threshold);

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

    // BH1750 조도 센서 관련
    bool initBH1750();
    float readLux();
    void closeBH1750();

    void yuyvToRgb(const unsigned char *yuyv, int width, int height, unsigned char *rgb);

    int m_fd;
    int m_width;
    int m_height;
    BufferInfo m_buffers[kMaxBuffer];
    int m_bufferCount;

    bool m_running;
    QMutex m_captureLock;
    QString m_pendingCapturePath;

    // BH1750 I2C
    int m_i2cFd;
    float m_luxThreshold;
};

#endif // ALARMCAMERATHREAD_H
