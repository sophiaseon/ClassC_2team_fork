#include "alarmcamerathread.h"
//hello
#include <QDebug>
#include <QDir>

#include <errno.h>
#include <fcntl.h>
#include <linux/videodev2.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

AlarmCameraThread::AlarmCameraThread(QObject *parent)
    : QThread(parent)
    , m_fd(-1)
    , m_width(640)
    , m_height(480)
    , m_bufferCount(0)
    , m_running(false)
    , m_i2cFd(-1)
    , m_luxThreshold(50.0f)
{}

AlarmCameraThread::~AlarmCameraThread()
{
    stop();
}

void AlarmCameraThread::stop()
{
    m_running = false;
    requestInterruption();
    wait(800);
}

void AlarmCameraThread::requestCapture(const QString &savePath)
{
    QMutexLocker locker(&m_captureLock);
    m_pendingCapturePath = savePath;
}

void AlarmCameraThread::setLuxThreshold(float lux)
{
    QMutexLocker locker(&m_captureLock);
    m_luxThreshold = lux;
}

void AlarmCameraThread::run()
{
    if (initCapture() < 0) {
        emit cameraError("Failed to initialize camera (/dev/video4)");
        return;
    }
    if (startCapture() < 0) {
        emit cameraError("Failed to start camera streaming");
        closeCapture();
        return;
    }

    if (!initBH1750()) {
        qWarning() << "[AlarmCameraThread] BH1750 unavailable; lux check disabled";
    }

    m_running = true;
    while (m_running && !isInterruptionRequested()) {
        if (captureFrame() < 0) break;
    }

    stopCapture();
    closeCapture();
    closeBH1750();
}

int AlarmCameraThread::initCapture()
{
    m_fd = ::open("/dev/video4", O_RDWR);
    if (m_fd < 0) {
        qWarning() << "[AlarmCameraThread] open(/dev/video4) failed, errno=" << errno;
        return -1;
    }

    struct v4l2_capability cap;
    memset(&cap, 0, sizeof(cap));
    if (::ioctl(m_fd, VIDIOC_QUERYCAP, &cap) < 0) {
        qWarning() << "[AlarmCameraThread] VIDIOC_QUERYCAP failed";
        return -1;
    }

    if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE) ||
        !(cap.capabilities & V4L2_CAP_STREAMING)) {
        qWarning() << "[AlarmCameraThread] camera does not support capture+streaming";
        return -1;
    }

    if (configureFormat() < 0) return -1;

    struct v4l2_requestbuffers req;
    memset(&req, 0, sizeof(req));
    req.count = kMaxBuffer;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;

    if (::ioctl(m_fd, VIDIOC_REQBUFS, &req) < 0) {
        qWarning() << "[AlarmCameraThread] VIDIOC_REQBUFS failed";
        return -1;
    }

    m_bufferCount = req.count;
    for (int i = 0; i < m_bufferCount; ++i) {
        struct v4l2_buffer buf;
        memset(&buf, 0, sizeof(buf));
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;

        if (::ioctl(m_fd, VIDIOC_QUERYBUF, &buf) < 0) {
            qWarning() << "[AlarmCameraThread] VIDIOC_QUERYBUF failed";
            return -1;
        }

        m_buffers[i].length = buf.length;
        m_buffers[i].start = ::mmap(nullptr, buf.length, PROT_READ | PROT_WRITE,
                                    MAP_SHARED, m_fd, buf.m.offset);
        if (m_buffers[i].start == MAP_FAILED) {
            m_buffers[i].start = nullptr;
            qWarning() << "[AlarmCameraThread] mmap failed";
            return -1;
        }

        if (::ioctl(m_fd, VIDIOC_QBUF, &buf) < 0) {
            qWarning() << "[AlarmCameraThread] initial VIDIOC_QBUF failed";
            return -1;
        }
    }

    return 0;
}

int AlarmCameraThread::configureFormat()
{
    struct v4l2_format fmt;
    memset(&fmt, 0, sizeof(fmt));
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = 640;
    fmt.fmt.pix.height = 480;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;

    if (::ioctl(m_fd, VIDIOC_S_FMT, &fmt) < 0) {
        qWarning() << "[AlarmCameraThread] VIDIOC_S_FMT failed";
        return -1;
    }

    m_width = fmt.fmt.pix.width;
    m_height = fmt.fmt.pix.height;
    return 0;
}

int AlarmCameraThread::startCapture()
{
    int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (::ioctl(m_fd, VIDIOC_STREAMON, &type) < 0) {
        qWarning() << "[AlarmCameraThread] VIDIOC_STREAMON failed";
        return -1;
    }
    return 0;
}

int AlarmCameraThread::captureFrame()
{
    struct v4l2_buffer buf;
    memset(&buf, 0, sizeof(buf));
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;

    if (::ioctl(m_fd, VIDIOC_DQBUF, &buf) < 0) {
        if (errno == EINTR) return 0;
        qWarning() << "[AlarmCameraThread] VIDIOC_DQBUF failed";
        return -1;
    }

    QImage frame(m_width, m_height, QImage::Format_RGB888);
    yuyvToRgb(static_cast<const unsigned char *>(m_buffers[buf.index].start),
              m_width, m_height, frame.bits());

    emit frameReady(frame.copy());

    QString savePath;
    float threshold;
    {
        QMutexLocker locker(&m_captureLock);
        savePath = m_pendingCapturePath;
        if (!savePath.isEmpty()) m_pendingCapturePath.clear();
        threshold = m_luxThreshold;
    }

    if (!savePath.isEmpty()) {
        const float lux = readLux();
        if (m_i2cFd >= 0 && lux >= 0.0f && lux < threshold) {
            qWarning() << "[AlarmCameraThread] capture rejected: lux=" << lux
                       << "< threshold=" << threshold;
            emit captureRejectedLowLight(lux, threshold);
        } else {
            QDir().mkpath(QFileInfo(savePath).absolutePath());
            const bool ok = frame.save(savePath, "JPG", 92);
            emit captureSaved(savePath, ok, ok ? QString() : QString("Failed to save image"));
        }
    }

    if (::ioctl(m_fd, VIDIOC_QBUF, &buf) < 0) {
        qWarning() << "[AlarmCameraThread] VIDIOC_QBUF failed";
        return -1;
    }

    return 0;
}

int AlarmCameraThread::stopCapture()
{
    if (m_fd < 0) return 0;
    int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (::ioctl(m_fd, VIDIOC_STREAMOFF, &type) < 0) {
        qWarning() << "[AlarmCameraThread] VIDIOC_STREAMOFF failed";
        return -1;
    }
    return 0;
}

// ──────────────────────────────────────────────
// BH1750 조도 센서 (I2C 주소 0x23, /dev/i2c-1)
// ──────────────────────────────────────────────

bool AlarmCameraThread::initBH1750()
{
    // 드라이버가 open() 시점에 Power On + 고해상도 연속 측정 모드 설정 및 대기를 수행한다.
    m_i2cFd = ::open("/dev/bh1750", O_RDONLY);
    if (m_i2cFd < 0) {
        qWarning() << "[AlarmCameraThread] open(/dev/bh1750) failed, errno=" << errno;
        return false;
    }

    QMutexLocker locker(&m_captureLock);
    qDebug() << "[AlarmCameraThread] BH1750 initialized (threshold:" << m_luxThreshold << "lux)";
    return true;
}

float AlarmCameraThread::readLux()
{
    if (m_i2cFd < 0) return -1.0f;

    // 드라이버가 (raw * 10) / 12 로 계산된 lux 값을 unsigned short 로 반환한다.
    unsigned short lux = 0;
    if (::read(m_i2cFd, &lux, sizeof(lux)) != static_cast<ssize_t>(sizeof(lux))) {
        qWarning() << "[AlarmCameraThread] BH1750 read failed";
        return -1.0f;
    }

    return static_cast<float>(lux);
}

void AlarmCameraThread::closeBH1750()
{
    if (m_i2cFd >= 0) {
        ::close(m_i2cFd);
        m_i2cFd = -1;
    }
}

void AlarmCameraThread::closeCapture()
{
    for (int i = 0; i < m_bufferCount; ++i) {
        if (m_buffers[i].start) {
            ::munmap(m_buffers[i].start, m_buffers[i].length);
            m_buffers[i].start = nullptr;
            m_buffers[i].length = 0;
        }
    }
    m_bufferCount = 0;

    if (m_fd >= 0) {
        ::close(m_fd);
        m_fd = -1;
    }
}

void AlarmCameraThread::yuyvToRgb(const unsigned char *yuyv, int width, int height, unsigned char *rgb)
{
    const long yuvSize = static_cast<long>(height) * width * 2;
    const long rgbSize = static_cast<long>(height) * width * 3;

    for (long i = 0, j = 0; i < rgbSize && j < yuvSize; i += 6, j += 4) {
        const int y0 = yuyv[j + 0];
        const int u0 = yuyv[j + 1];
        const int y1 = yuyv[j + 2];
        const int v0 = yuyv[j + 3];

        auto clamp = [](float x) -> unsigned char {
            if (x < 0.0f) return 0;
            if (x > 255.0f) return 255;
            return static_cast<unsigned char>(x);
        };

        float r = y0 + 1.4065f * (v0 - 128);
        float g = y0 - 0.3455f * (u0 - 128) - 0.7169f * (v0 - 128);
        float b = y0 + 1.1790f * (u0 - 128);
        rgb[i + 0] = clamp(r);
        rgb[i + 1] = clamp(g);
        rgb[i + 2] = clamp(b);

        r = y1 + 1.4065f * (v0 - 128);
        g = y1 - 0.3455f * (u0 - 128) - 0.7169f * (v0 - 128);
        b = y1 + 1.1790f * (u0 - 128);
        rgb[i + 3] = clamp(r);
        rgb[i + 4] = clamp(g);
        rgb[i + 5] = clamp(b);
    }
}
