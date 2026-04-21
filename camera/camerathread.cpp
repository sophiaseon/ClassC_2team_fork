#include "camerathread.h"

CameraThread::CameraThread(QObject *parent)
    : QThread(parent)
{
    videodev.fd = -1;
    frame_count = 0;
    frame_devisor = 1;
    m_stop = 0;
    m_uiReady = 1;
}

void CameraThread::requestStop()
{
    m_stop = 1;
}

void CameraThread::run()
{
    if (initCapture() < 0)
        return;

    if (startCapture() < 0)
        return;

    while (!m_stop) {
        if (captureFrame() < 0)
            break;
    }

    stopCapture();
    closeCapture();
}

void CameraThread::vidioc_enuminput(int fd)
{
    struct v4l2_input input;
    memset(&input, 0, sizeof(input));
    input.index = 0;
    while (ioctl(fd, VIDIOC_ENUMINPUT, &input) == 0) {
        qDebug() << "input name =" << (char *)input.name
                 << " type =" << input.type
                 << " status =" << input.status
                 << " std =" << input.std;
        input.index++;
    }
}

int CameraThread::initCapture()
{
    if (videodev.fd > 0)
        return 0;

    int err;
    int fd = open("/dev/video7", O_RDWR);
    if (fd < 0) {
        qWarning() << "open /dev/video7 fail " << fd;
        return fd;
    }
    videodev.fd = fd;

    struct v4l2_capability cap;
    if ((err = ioctl(fd, VIDIOC_QUERYCAP, &cap)) < 0) {
        qWarning() << "VIDIOC_QUERYCAP error " << err;
        goto err1;
    }
    qDebug() << "card =" << (char *)cap.card
             << " driver =" << (char *)cap.driver
             << " bus =" << (char *)cap.bus_info;

    if (cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)
        qDebug() << "/dev/video0: Capable off capture";
    else {
        qWarning() << "/dev/video0: Not capable of capture";
        goto err1;
    }

    if (cap.capabilities & V4L2_CAP_STREAMING)
        qDebug() << "/dev/video0: Capable of streaming";
    else {
        qWarning() << "/dev/video0: Not capable of streaming";
        goto err1;
    }

    if ((err = subInitCapture()) < 0)
        goto err1;

    struct v4l2_requestbuffers reqbuf;
    reqbuf.count = 5;
    reqbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    reqbuf.memory = V4L2_MEMORY_MMAP;
    if ((err = ioctl(fd, VIDIOC_REQBUFS, &reqbuf)) < 0) {
        qWarning() << "Cannot allocate memory";
        goto err1;
    }
    videodev.numbuffer = reqbuf.count;
    qDebug() << "buffer actually allocated" << reqbuf.count;

    uint i;
    struct v4l2_buffer buf;
    memset(&buf, 0, sizeof(buf));
    for (i = 0; i < reqbuf.count; i++) {
        buf.type = reqbuf.type;
        buf.index = i;
        buf.memory = reqbuf.memory;
        err = ioctl(fd, VIDIOC_QUERYBUF, &buf);
        Q_ASSERT(err == 0);

        videodev.buff_info[i].length = buf.length;
        videodev.buff_info[i].index = i;
        videodev.buff_info[i].start =
                (uchar *)mmap(NULL, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, buf.m.offset);

        Q_ASSERT(videodev.buff_info[i].start != MAP_FAILED);

        memset((void *) videodev.buff_info[i].start, 0x80,
               videodev.buff_info[i].length);

        err = ioctl(fd, VIDIOC_QBUF, &buf);
        Q_ASSERT(err == 0);
    }

    return 0;

err1:
    close(fd);
    return err;
}

int CameraThread::startCapture()
{
    int a, ret;

    /* Start Streaming. on capture device */
    a = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    ret = ioctl(videodev.fd, VIDIOC_STREAMON, &a);
    if (ret < 0) {
        qDebug() << "capture VIDIOC_STREAMON error fd=" << videodev.fd;
        return ret;
    }
    qDebug() << "Stream on...";

    return 0;
}

int CameraThread::captureFrame()
{
    int ret;
    struct v4l2_buffer buf;

    memset(&buf, 0, sizeof(buf));
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;

    /* Dequeue capture buffer (blocking) */
    ret = ioctl(videodev.fd, VIDIOC_DQBUF, &buf);
    if (ret < 0) {
        qWarning() << "Cap VIDIOC_DQBUF error" << errno;
        return ret;
    }

    if (frame_count++ % frame_devisor == 0) {
        // UI가 이전 프레임을 아직 처리 중이면 드롭
        if (m_uiReady.testAndSetAcquire(1, 0)) {
            // YUYV→RGB 변환을 카메라 스레드에서 수행
            int w = videodev.cap_width;
            int h = videodev.cap_height;
            const uchar *yuyv = reinterpret_cast<const uchar *>(videodev.buff_info[buf.index].start);
            QImage img(w, h, QImage::Format_RGB888);
            for (int i = 0, j = 0; i < h * w * 3 && j < h * w * 2; i += 6, j += 4) {
                auto cvt = [](int y, int u, int v, uchar *out) {
                    auto clamp = [](float x) -> uchar {
                        return x < 0 ? 0 : (x > 255 ? 255 : (uchar)x);
                    };
                    out[0] = clamp(y + 1.4065f * (v - 128));
                    out[1] = clamp(y - 0.3455f * (u - 128) - 0.7169f * (v - 128));
                    out[2] = clamp(y + 1.1790f * (u - 128));
                };
                uchar *bits = img.bits() + i;
                cvt(yuyv[j], yuyv[j+1], yuyv[j+3], bits);
                cvt(yuyv[j+2], yuyv[j+1], yuyv[j+3], bits + 3);
            }

            ret = ioctl(videodev.fd, VIDIOC_QBUF, &buf);
            if (ret < 0) { qDebug() << "Cap VIDIOC_QBUF"; return ret; }

            emit send_image(img);
        } else {
            // UI 바쁨: 버퍼만 반납하고 프레임 드롭
            ret = ioctl(videodev.fd, VIDIOC_QBUF, &buf);
            if (ret < 0) { qDebug() << "Cap VIDIOC_QBUF"; return ret; }
        }
    } else {
        ret = ioctl(videodev.fd, VIDIOC_QBUF, &buf);
        if (ret < 0) {
            qDebug() << "Cap VIDIOC_QBUF";
            return ret;
        }
    }

    return 0;
}

int CameraThread::stopCapture()
{
    int a, ret;

    qDebug() << "Stream off!!\n";

    a = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    ret = ioctl(videodev.fd, VIDIOC_STREAMOFF, &a);
    if (ret < 0) {
        qDebug() << "VIDIOC_STREAMOFF";
        return ret;
    }

    return 0;
}

void CameraThread::closeCapture()
{
    int i;
    struct buf_info *buff_info;

    /* Un-map the buffers */
    for (i = 0; i < videodev.numbuffer; i++){
        buff_info = &videodev.buff_info[i];
        if (buff_info->start) {
            munmap(buff_info->start, buff_info->length);
            buff_info->start = NULL;
        }
    }

    if (videodev.fd >= 0) {
        close(videodev.fd);
        videodev.fd = -1;
    }
}

int CameraThread::subInitCapture()
{
    int fd = videodev.fd;

    struct v4l2_dbg_chip_info chip;
    if (ioctl(fd, VIDIOC_DBG_G_CHIP_INFO, &chip) < 0)
        qWarning() << "VIDIOC_DBG_G_CHIP_INFO error " << errno;
    else
        qDebug() << "chip info " << chip.name;

    bool support_fmt;
    struct v4l2_fmtdesc ffmt;
    memset(&ffmt, 0, sizeof(ffmt));
    ffmt.index = 0;
    ffmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    support_fmt = false;
    while (ioctl(fd, VIDIOC_ENUM_FMT, &ffmt) == 0) {
        qDebug() << "fmt" << ffmt.pixelformat << (char *)ffmt.description;
        if (ffmt.pixelformat == V4L2_PIX_FMT_YUYV)
            support_fmt = true;
        ffmt.index++;
    }
    if (!support_fmt) {
        qWarning() << "V4L2_PIX_FMT_YUYV is not supported by this camera";
        return -1;
    }

    bool support_320x240;
    struct v4l2_frmsizeenum fsize;
    memset(&fsize, 0, sizeof(fsize));
    fsize.index = 0;
    fsize.pixel_format = V4L2_PIX_FMT_YUYV;
    support_320x240 = false;
    while (ioctl(fd, VIDIOC_ENUM_FRAMESIZES, &fsize) == 0) {
        qDebug() << "frame size " << fsize.discrete.width << fsize.discrete.height;
        if (fsize.discrete.width == 320 && fsize.discrete.height == 240)
            support_320x240 = true;
        fsize.index++;
    }
    if (!support_320x240) {
        qWarning() << "frame size 320x240 is not supported by this camera";
        return -1;
    }

    vidioc_enuminput(fd);

    int index;
    if (ioctl(fd, VIDIOC_G_INPUT, &index) < 0)
        qWarning() << "VIDIOC_G_INPUT fail" << errno;
    else
        qDebug() << "current input index =" << index;

    struct v4l2_format fmt;
    memset(&fmt, 0, sizeof(fmt));
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(fd, VIDIOC_G_FMT, &fmt) < 0)
        qWarning() << "VIDIOC_G_FMT fail" << errno;
    else
        qDebug() << "fmt width =" << fmt.fmt.pix.width
                 << " height =" << fmt.fmt.pix.height
                 << " pfmt =" << fmt.fmt.pix.pixelformat;

    fmt.fmt.pix.width = 320;
    fmt.fmt.pix.height = 240;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
    if (ioctl(fd, VIDIOC_S_FMT, &fmt) < 0)
        qWarning() << "VIDIOC_S_FMT fail" << errno;
    else
        qDebug() << "VIDIOC_S_FMT success";

    memset(&fmt, 0, sizeof(fmt));
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(fd, VIDIOC_G_FMT, &fmt) < 0)
        qWarning() << "VIDIOC_G_FMT fail" << errno;
    else
        qDebug() << "fmt width =" << fmt.fmt.pix.width
                 << " height =" << fmt.fmt.pix.height
                 << " pfmt =" << fmt.fmt.pix.pixelformat;
    Q_ASSERT(fmt.fmt.pix.width == 320);
    Q_ASSERT(fmt.fmt.pix.height == 240);
    Q_ASSERT(fmt.fmt.pix.pixelformat == V4L2_PIX_FMT_YUYV);

    videodev.cap_width = fmt.fmt.pix.width;
    videodev.cap_height = fmt.fmt.pix.height;

    // 프레임레이트 명시적 설정 (10fps - USB 대역폭 절약, NFS 공존 가능)
    struct v4l2_streamparm parm;
    memset(&parm, 0, sizeof(parm));
    parm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    parm.parm.capture.timeperframe.numerator = 1;
    parm.parm.capture.timeperframe.denominator = 10;
    if (ioctl(fd, VIDIOC_S_PARM, &parm) < 0)
        qWarning() << "VIDIOC_S_PARM fail" << errno;
    else
        qDebug() << "framerate set to 10fps";

    return 0;
}
