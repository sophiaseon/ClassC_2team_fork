#include "ultrasonicwatcher.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/select.h>
#include <unistd.h>

#include <QDebug>
#include <QMetaType>
#include <QString>

#define HCSR04_DEVICE    "/dev/hcsr04_array"
#define POLL_INTERVAL_US  300000   // 300 ms between measurements

// ── UltrasonicWorker ──────────────────────────────────────────────────────────

UltrasonicWorker::UltrasonicWorker(int stopPipeRead, QObject *parent)
    : QObject(parent), m_stopPipeRead(stopPipeRead)
{}

void UltrasonicWorker::run()
{
    qDebug() << "[UltrasonicWorker] thread started, polling" << HCSR04_DEVICE;

    while (true) {
        // Sleep for POLL_INTERVAL_US, but wake immediately if stop is signalled
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(m_stopPipeRead, &rfds);

        struct timeval tv;
        tv.tv_sec  = 0;
        tv.tv_usec = POLL_INTERVAL_US;

        int ret = ::select(m_stopPipeRead + 1, &rfds, nullptr, nullptr, &tv);
        if (ret < 0) {
            if (errno == EINTR) continue;
            qDebug() << "[UltrasonicWorker] select() error, errno=" << errno;
            break;
        }
        if (ret > 0 && FD_ISSET(m_stopPipeRead, &rfds))
            break; // stop requested

        // Open → read → close (driver triggers measurement on each read)
        int fd = ::open(HCSR04_DEVICE, O_RDONLY);
        if (fd < 0) {
            qDebug() << "[UltrasonicWorker] open(" HCSR04_DEVICE ") failed, errno=" << errno
                     << "(" << ::strerror(errno) << ")";
            emit distancesRead(QVector<int>(4, -1));
            continue;
        }

        char buf[256];
        ::memset(buf, 0, sizeof(buf));
        ssize_t n = ::read(fd, buf, sizeof(buf) - 1);
        ::close(fd);

        if (n < 0) {
            qDebug() << "[UltrasonicWorker] read() failed, errno=" << errno
                     << "(" << ::strerror(errno) << ")";
            emit distancesRead(QVector<int>(4, -1));
            continue;
        }
        if (n == 0) {
            qDebug() << "[UltrasonicWorker] read() returned 0 (EOF / ppos issue)";
            emit distancesRead(QVector<int>(4, -1));
            continue;
        }
        buf[n] = '\0';

        // Parse lines: "sensor0:15\n" or "sensor1:error=-110\n"
        QVector<int> distances(4, -1);
        char *line = buf;
        while (line && *line) {
            char *next = ::strchr(line, '\n');
            if (next) *next = '\0';

            int idx = -1, dist = -1;
            // Normal case: "sensorN:D"
            if (::sscanf(line, "sensor%d:%d", &idx, &dist) == 2) {
                if (idx >= 0 && idx < 4)
                    distances[idx] = dist;
            }
            // Error case: "sensorN:error=D" — sensor exists but failed
            // distances[idx] stays -1, which is correct

            line = next ? (next + 1) : nullptr;
        }

        // Debug: print all distances to CLI every poll
        qDebug().nospace()
            << "[Ultrasonic] S1=" << distances[0] << "cm"
            << "  S2=" << distances[1] << "cm"
            << "  S3=" << distances[2] << "cm"
            << "  S4=" << distances[3] << "cm"
            << "  (raw: " << QString::fromLatin1(buf).replace('\n', '|') << ")";

        emit distancesRead(distances);
    }

    qDebug() << "[UltrasonicWorker] thread exiting";
}

// ── UltrasonicWatcher ─────────────────────────────────────────────────────────

UltrasonicWatcher::UltrasonicWatcher(QObject *parent)
    : QObject(parent)
    , m_worker(nullptr)
{
    qRegisterMetaType<QVector<int>>("QVector<int>");

    m_stopPipe[0] = m_stopPipe[1] = -1;

    if (::pipe(m_stopPipe) < 0) {
        qDebug() << "[UltrasonicWatcher] pipe() FAILED, errno=" << errno;
        return;
    }

    m_worker = new UltrasonicWorker(m_stopPipe[0]);
    m_worker->moveToThread(&m_thread);

    connect(&m_thread, &QThread::started,  m_worker, &UltrasonicWorker::run);
    connect(m_worker,  &UltrasonicWorker::distancesRead,
            this,      &UltrasonicWatcher::distancesRead);

    m_thread.start();
    qDebug() << "[UltrasonicWatcher] started polling" << HCSR04_DEVICE;
}

UltrasonicWatcher::~UltrasonicWatcher()
{
    // Signal the worker's select() to wake and exit
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
    if (m_stopPipe[0] >= 0) ::close(m_stopPipe[0]);
    if (m_stopPipe[1] >= 0) ::close(m_stopPipe[1]);
}
