#include "buttonwatcher.h"

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <QDebug>

ButtonWatcher::ButtonWatcher(QObject *parent)
    : QObject(parent)
    , m_fd(::open("/dev/my_dev0", O_RDONLY | O_NONBLOCK))
{
    if (m_fd < 0) {
        qDebug() << "[ButtonWatcher] open(/dev/my_dev0) FAILED, errno=" << errno
                 << " — 커널 모듈이 로드됐는지, mknod로 장치 파일이 생성됐는지 확인하세요";
        return;
    }
    qDebug() << "[ButtonWatcher] open(/dev/my_dev0) OK, fd=" << m_fd;

    connect(&m_timer, &QTimer::timeout, this, &ButtonWatcher::poll);
    m_timer.start(100); // poll every 100 ms
}

ButtonWatcher::~ButtonWatcher()
{
    m_timer.stop();
    if (m_fd >= 0) ::close(m_fd);
}

void ButtonWatcher::poll()
{
    if (m_fd < 0) return;

    char buf[1];
    ssize_t r = ::read(m_fd, buf, 1);
    if (r == 1) {
        qDebug() << "[ButtonWatcher] button press detected! emitting buttonPressed()";
        // Drain any extra counts accumulated between polls
        while (::read(m_fd, buf, 1) == 1) {}
        emit buttonPressed();
    } else if (r < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
        qDebug() << "[ButtonWatcher] read() unexpected error, errno=" << errno;
    }
}
