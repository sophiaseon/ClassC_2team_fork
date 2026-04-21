#include "alarmcamerathread.h"

#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QProcess>

#include <errno.h>
#include <string.h>
#include <sys/select.h>

// ── Ethernet link control ─────────────────────────────────────────────────────
// USB camera and Ethernet share the same USB bus on the Raspberry Pi.
// Bringing the Ethernet link down while the camera is active frees up
// bus bandwidth and prevents frame drops / stalls.
// Adjust ETH_IFACE if your board uses a different interface name (e.g. end0).
static const char *ETH_IFACE = "eth0";

static void setEthernetLink(bool up)
{
    const QString state = up ? QStringLiteral("up") : QStringLiteral("down");
    int ret = QProcess::execute("ip", {"link", "set", ETH_IFACE, state});
    if (ret != 0)
        qWarning() << "[Camera] 'ip link set" << ETH_IFACE << state << "' returned" << ret;
    else
        qDebug()  << "[Camera] Ethernet" << ETH_IFACE << state;
}

// ── ctor / dtor ───────────────────────────────────────────────────────────────

AlarmCameraThread::AlarmCameraThread(QObject *parent)
    : QThread(parent)
    , m_uiReady(1)
    , m_running(false)
    , m_lastEmitMs(0)
    , m_i2cFd(-1)
    , m_luxThreshold(50.0f)
{
    m_videodev.fd = -1;
    m_videodev.cap_width  = 320;
    m_videodev.cap_height = 240;
    m_videodev.numbuffer  = 0;
    for (int i = 0; i < CAPTURE_MAX_BUFFER; ++i) {
        m_videodev.buff_info[i].index  = i;
        m_videodev.buff_info[i].length = 0;
        m_videodev.buff_info[i].start  = nullptr;
    }
}

AlarmCameraThread::~AlarmCameraThread()
{
    stop();
}

// ── public API ────────────────────────────────────────────────────────────────

void AlarmCameraThread::stop()
{
    m_running = false;
    requestInterruption();
    // STREAMOFF unblocks the blocking VIDIOC_DQBUF call
    if (m_videodev.fd >= 0) {
        int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        ioctl(m_videodev.fd, VIDIOC_STREAMOFF, &type);
    }
    wait(3000);
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

// ── run ───────────────────────────────────────────────────────────────────────

void AlarmCameraThread::run()
{
    // Bring Ethernet down so the USB camera gets full bus bandwidth.
    setEthernetLink(false);

    // BH1750 open() blocks ~8s on this driver — do it before STREAMON
    if (!initBH1750())
        qWarning() << "[Camera] BH1750 unavailable, lux check disabled";

    if (initCapture() < 0) {
        emit cameraError("Camera init failed (/dev/video0, /dev/video1)");
        setEthernetLink(true);
        return;
    }

    if (startCapture() < 0) {
        emit cameraError("Camera streaming start failed");
        closeCapture();
        setEthernetLink(true);
        return;
    }

    qDebug() << "[Camera] streaming started";
    emit statusUpdate("Camera streaming...");
    m_running = true;

    while (m_running && !isInterruptionRequested()) {
        if (captureFrame() < 0)
            break;
    }

    stopCapture();
    closeCapture();
    closeBH1750();

    // Restore Ethernet now that the camera is done.
    setEthernetLink(true);
}

// ── xioctl ───────────────────────────────────────────────────────────────────

static int xioctl(int fd, int request, void *arg)
{
    int r;
    do { r = ioctl(fd, request, arg); } while (r < 0 && errno == EINTR);
    return r;
}

// ── initCapture ───────────────────────────────────────────────────────────────
// Matches reference camerathread.cpp structure exactly.

int AlarmCameraThread::initCapture()
{
    if (m_videodev.fd >= 0)
        return 0;

    int fd = open("/dev/video7", O_RDWR | O_NONBLOCK | O_CLOEXEC);
    if (fd < 0) {
        qWarning() << "[Camera] open /dev/video7 failed:" << strerror(errno);
        return -1;
    }
    videodev_init:
    m_videodev.fd = fd;

    // QUERYCAP
    struct v4l2_capability cap;
    if (xioctl(fd, VIDIOC_QUERYCAP, &cap) < 0) {
        qWarning() << "[Camera] QUERYCAP failed";
        goto err;
    }
    qDebug() << "[Camera] card =" << (char *)cap.card
             << "driver =" << (char *)cap.driver
             << "bus =" << (char *)cap.bus_info;
    if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE) ||
        !(cap.capabilities & V4L2_CAP_STREAMING)) {
        qWarning() << "[Camera] device does not support capture/streaming";
        goto err;
    }

    // CROPCAP / S_CROP — reset to default, same as v4l2test (errors ignored for UVC)
    {
        struct v4l2_cropcap cropcap;
        memset(&cropcap, 0, sizeof(cropcap));
        cropcap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        if (xioctl(fd, VIDIOC_CROPCAP, &cropcap) == 0) {
            struct v4l2_crop crop;
            memset(&crop, 0, sizeof(crop));
            crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            crop.c    = cropcap.defrect;
            xioctl(fd, VIDIOC_S_CROP, &crop);  // errors ignored
        }
    }

    // S_FMT — 640x480 same as v4l2test (320x240 is unreliable on kernel 6.12 UVC)
    {
        struct v4l2_format fmt;
        memset(&fmt, 0, sizeof(fmt));
        fmt.type                   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        fmt.fmt.pix.width          = 640;
        fmt.fmt.pix.height         = 480;
        fmt.fmt.pix.sizeimage      = 640 * 480 * 2;
        fmt.fmt.pix.pixelformat    = V4L2_PIX_FMT_YUYV;
        fmt.fmt.pix.field          = V4L2_FIELD_NONE;
        if (xioctl(fd, VIDIOC_S_FMT, &fmt) < 0) {
            qWarning() << "[Camera] S_FMT failed:" << strerror(errno);
            goto err;
        }
        m_videodev.cap_width  = fmt.fmt.pix.width;
        m_videodev.cap_height = fmt.fmt.pix.height;
        qDebug() << "[Camera] format:" << fmt.fmt.pix.width << "x" << fmt.fmt.pix.height
                 << "sizeimage=" << fmt.fmt.pix.sizeimage
                 << "field=" << fmt.fmt.pix.field;
    }

    // REQBUFS — 5 buffers like reference
    {
        struct v4l2_requestbuffers req;
        memset(&req, 0, sizeof(req));
        req.count  = CAPTURE_MAX_BUFFER;
        req.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        req.memory = V4L2_MEMORY_MMAP;
        if (xioctl(fd, VIDIOC_REQBUFS, &req) < 0) {
            qWarning() << "[Camera] REQBUFS failed:" << strerror(errno);
            goto err;
        }
        m_videodev.numbuffer = req.count;
        qDebug() << "[Camera] buffers allocated:" << req.count;
    }

    // QUERYBUF + mmap only — do NOT QBUF here.
    // QBUF is done in startCapture() immediately before STREAMON,
    // matching v4l2test's start_capturing() order exactly.
    for (int i = 0; i < m_videodev.numbuffer; ++i) {
        struct v4l2_buffer buf;
        memset(&buf, 0, sizeof(buf));
        buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index  = i;
        if (xioctl(fd, VIDIOC_QUERYBUF, &buf) < 0) {
            qWarning() << "[Camera] QUERYBUF[" << i << "] failed";
            goto err;
        }
        m_videodev.buff_info[i].length = buf.length;
        m_videodev.buff_info[i].index  = i;
        m_videodev.buff_info[i].start  =
            mmap(nullptr, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, buf.m.offset);
        if (m_videodev.buff_info[i].start == MAP_FAILED) {
            m_videodev.buff_info[i].start = nullptr;
            qWarning() << "[Camera] mmap[" << i << "] failed";
            goto err;
        }
    }
    return 0;

err:
    close(fd);
    m_videodev.fd = -1;
    return -1;
}

// ── startCapture ─────────────────────────────────────────────────────────────

int AlarmCameraThread::startCapture()
{
    // QBUF all buffers immediately before STREAMON — same as v4l2test start_capturing().
    // Kernel 6.12 UVC requires QBUF and STREAMON to be consecutive.
    for (int i = 0; i < m_videodev.numbuffer; ++i) {
        struct v4l2_buffer buf;
        memset(&buf, 0, sizeof(buf));
        buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index  = i;
        if (xioctl(m_videodev.fd, VIDIOC_QBUF, &buf) < 0) {
            qWarning() << "[Camera] QBUF[" << i << "] failed:" << strerror(errno);
            return -1;
        }
    }

    int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (xioctl(m_videodev.fd, VIDIOC_STREAMON, &type) < 0) {
        qWarning() << "[Camera] STREAMON failed:" << strerror(errno);
        return -1;
    }
    qDebug() << "[Camera] Stream on...";
    return 0;
}

// ── captureFrame ──────────────────────────────────────────────────────────────

int AlarmCameraThread::captureFrame()
{
    // select() + O_NONBLOCK — matches v4l2test mainloop exactly.
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(m_videodev.fd, &fds);
    struct timeval tv = {2, 0};  // 2s, same as v4l2test
    int r = select(m_videodev.fd + 1, &fds, nullptr, nullptr, &tv);
    if (r == 0) {
        // timeout — v4l2test exits here, we retry (continuous streaming)
        return 0;
    }
    if (r < 0) {
        if (errno == EINTR) return 0;
        if (!m_running) return -1;
        qWarning() << "[Cam] select error:" << strerror(errno);
        return -1;
    }

    struct v4l2_buffer buf;
    memset(&buf, 0, sizeof(buf));
    buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;

    int ret = ioctl(m_videodev.fd, VIDIOC_DQBUF, &buf);
    if (ret < 0) {
        if (errno == EAGAIN) return 0;  // spurious wakeup, retry
        if (!m_running) return -1;      // stopped cleanly
        qWarning() << "[Cam] DQBUF failed:" << strerror(errno);
        return -1;
    }

    // ── diagnostic counters ───────────────────────────────────────────────────
    static int s_total = 0, s_dropped = 0;
    ++s_total;
    if (s_total % 30 == 0)
        qDebug() << "[Cam] frames dequeued=" << s_total
                 << " dropped(uibusy)=" << s_dropped
                 << " uiReady=" << m_uiReady.loadAcquire();

    // Check pending capture request
    QString savePath;
    float   threshold;
    {
        QMutexLocker locker(&m_captureLock);
        savePath  = m_pendingCapturePath;
        if (!savePath.isEmpty()) m_pendingCapturePath.clear();
        threshold = m_luxThreshold;
    }
    const bool captureNeeded = !savePath.isEmpty();

    const bool uiReady = m_uiReady.testAndSetAcquire(1, 0);

    if (uiReady || captureNeeded) {
        const int w = m_videodev.cap_width;
        const int h = m_videodev.cap_height;
        const uchar *yuyv = reinterpret_cast<const uchar *>(m_videodev.buff_info[buf.index].start);
        QImage frame(w, h, QImage::Format_RGB32);
        uint *pixels = reinterpret_cast<uint *>(frame.bits());
        const int total = w * h / 2;
        for (int p = 0; p < total; ++p) {
            int y0 = yuyv[p*4 + 0];
            int u  = yuyv[p*4 + 1] - 128;
            int y1 = yuyv[p*4 + 2];
            int v  = yuyv[p*4 + 3] - 128;
            auto clamp = [](int x) -> uint { return x < 0 ? 0u : x > 255 ? 255u : (uint)x; };
            int ru = 92241 * v / 65536;
            int gu = -22612 * u / 65536;
            int gv = -46984 * v / 65536;
            int bu = 77318 * u / 65536;
            pixels[p*2]   = 0xFF000000 | clamp(y0+ru)<<16 | clamp(y0+gu+gv)<<8 | clamp(y0+bu);
            pixels[p*2+1] = 0xFF000000 | clamp(y1+ru)<<16 | clamp(y1+gu+gv)<<8 | clamp(y1+bu);
        }

        ioctl(m_videodev.fd, VIDIOC_QBUF, &buf);

        if (uiReady)
            emit frameReady(frame);

        if (captureNeeded) {
            const float lux = readLux();
            if (m_i2cFd >= 0 && lux >= 0.0f && lux < threshold) {
                emit captureRejectedLowLight(lux, threshold);
            } else {
                QDir().mkpath(QFileInfo(savePath).absolutePath());
                const bool ok = frame.save(savePath, "JPG", 92);
                emit captureSaved(savePath, ok,
                                  ok ? QString() : QString("Failed to save image"));
            }
        }
    } else {
        ++s_dropped;
        ioctl(m_videodev.fd, VIDIOC_QBUF, &buf);
    }

    return 0;
}

// ── stopCapture ───────────────────────────────────────────────────────────────

int AlarmCameraThread::stopCapture()
{
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (xioctl(m_videodev.fd, VIDIOC_STREAMOFF, &type) < 0) {
        qWarning() << "[Camera] STREAMOFF failed:" << strerror(errno);
        return -1;
    }
    return 0;
}

// ── closeCapture ──────────────────────────────────────────────────────────────

void AlarmCameraThread::closeCapture()
{
    for (int i = 0; i < CAPTURE_MAX_BUFFER; ++i) {
        if (m_videodev.buff_info[i].start) {
            munmap(m_videodev.buff_info[i].start, m_videodev.buff_info[i].length);
            m_videodev.buff_info[i].start  = nullptr;
            m_videodev.buff_info[i].length = 0;
        }
    }
    if (m_videodev.fd >= 0) {
        close(m_videodev.fd);
        m_videodev.fd = -1;
    }
}

// ── BH1750 ────────────────────────────────────────────────────────────────────

bool AlarmCameraThread::initBH1750()
{
    m_i2cFd = open("/dev/bh1750", O_RDONLY | O_CLOEXEC);
    if (m_i2cFd < 0) {
        qWarning() << "[Camera] open(/dev/bh1750) failed:" << strerror(errno);
        return false;
    }
    qDebug() << "[Camera] BH1750 initialized (threshold:" << m_luxThreshold << "lux)";
    return true;
}

float AlarmCameraThread::readLux()
{
    if (m_i2cFd < 0) return -1.0f;
    unsigned short lux = 0;
    if (::read(m_i2cFd, &lux, sizeof(lux)) != (ssize_t)sizeof(lux)) {
        qWarning() << "[Camera] BH1750 read failed";
        return -1.0f;
    }
    qDebug() << "[Camera] BH1750 lux read:" << lux;
    return (float)lux;
}

void AlarmCameraThread::closeBH1750()
{
    if (m_i2cFd >= 0) {
        close(m_i2cFd);
        m_i2cFd = -1;
    }
}
