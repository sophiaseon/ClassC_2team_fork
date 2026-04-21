#include "buttonwatcher.h"

#include <errno.h>
#include <fcntl.h>
#include <sys/select.h>
#include <unistd.h>
#include <QDebug>

// ── ButtonWorker ──────────────────────────────────────────────────────────────

ButtonWorker::ButtonWorker(int fd, int stopPipeRead, QObject *parent)
    : QObject(parent), m_fd(fd), m_stopPipeRead(stopPipeRead)
{}

void ButtonWorker::run()
{
    while (true) {
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(m_fd, &rfds);
        FD_SET(m_stopPipeRead, &rfds);
        int nfds = (m_fd > m_stopPipeRead ? m_fd : m_stopPipeRead) + 1;

        int ret = ::select(nfds, &rfds, nullptr, nullptr, nullptr);
        if (ret < 0) {
            if (errno == EINTR) continue;
            qDebug() << "[ButtonWorker] select() error, errno=" << errno;
            break;
        }

        // Stop signal
        if (FD_ISSET(m_stopPipeRead, &rfds))
            break;

        // Button pressed — blocking read
        if (FD_ISSET(m_fd, &rfds)) {
            char buf[64];
            ssize_t r = ::read(m_fd, buf, sizeof(buf));
            if (r > 0) {
                qDebug() << "[ButtonWorker] button press detected!";
                emit buttonPressed();
            } else if (r < 0) {
                qDebug() << "[ButtonWorker] read() error, errno=" << errno;
                break;
            }
        }
    }
}

// ── ButtonWatcher ─────────────────────────────────────────────────────────────

ButtonWatcher::ButtonWatcher(QObject *parent)
    : QObject(parent)
    , m_fd(::open("/dev/mydev", O_RDONLY))
    , m_worker(nullptr)
{
    m_stopPipe[0] = m_stopPipe[1] = -1;

    if (m_fd < 0) {
        qDebug() << "[ButtonWatcher] open(/dev/mydev) FAILED, errno=" << errno
                 << " — check if kernel module is loaded and device file exists (mknod)";
        return;
    }
    qDebug() << "[ButtonWatcher] open(/dev/mydev) OK, fd=" << m_fd;

    if (::pipe(m_stopPipe) < 0) {
        qDebug() << "[ButtonWatcher] pipe() FAILED, errno=" << errno;
        return;
    }

    m_worker = new ButtonWorker(m_fd, m_stopPipe[0]);
    m_worker->moveToThread(&m_thread);
    connect(&m_thread, &QThread::started,  m_worker, &ButtonWorker::run);
    connect(m_worker,  &ButtonWorker::buttonPressed,
            this,      &ButtonWatcher::buttonPressed);

    m_thread.start();
}

ButtonWatcher::~ButtonWatcher()
{
    // Write to the pipe to unblock select() in the worker
    if (m_stopPipe[1] >= 0) {
        char c = 1;
        (void)::write(m_stopPipe[1], &c, 1);
    }
    m_thread.quit();
    if (!m_thread.wait(3000)) {
        m_thread.terminate();
        m_thread.wait(1000);
    }
    delete m_worker;
    if (m_fd         >= 0) ::close(m_fd);
    if (m_stopPipe[0] >= 0) ::close(m_stopPipe[0]);
    if (m_stopPipe[1] >= 0) ::close(m_stopPipe[1]);
}
