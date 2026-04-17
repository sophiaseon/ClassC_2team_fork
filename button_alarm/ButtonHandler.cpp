#include "ButtonHandler.h"

#include <QDebug>
#include <QRegularExpression>

// Linux input subsystem 헤더 (Raspberry Pi / Linux 전용)
#ifdef Q_OS_LINUX
#  include <fcntl.h>
#  include <unistd.h>
#  include <cerrno>
#  include <cstring>
#endif

ButtonHandler::ButtonHandler(const QString &devicePath, int debounceMs, QObject *parent)
    : QObject(parent)
    , m_devicePath(devicePath)
    , m_debounceMs(debounceMs)
{
}

ButtonHandler::~ButtonHandler()
{
    close();
}

bool ButtonHandler::open()
{
#ifdef Q_OS_LINUX
    m_fd = ::open(m_devicePath.toLocal8Bit().constData(), O_RDONLY | O_NONBLOCK);
    if (m_fd < 0) {
        const QString msg = QString("버튼 디바이스 열기 실패: %1 (%2)")
                                .arg(m_devicePath)
                                .arg(QString::fromLocal8Bit(std::strerror(errno)));
        qWarning() << msg;
        emit deviceError(msg);
        return false;
    }

    m_pollTimer = new QTimer(this);
    connect(m_pollTimer, &QTimer::timeout, this, &ButtonHandler::pollDevice);

    // Drain all existing buffered kmsg entries so we don't count old button presses
    {
        char drain[512];
        while (::read(m_fd, drain, sizeof(drain)) > 0) {}
    }
    // Initialize lastCount from current state
    m_lastCount = -1;

    m_pollTimer->start(10);  // poll every 10ms

    m_debounceTimer.invalidate();
    qDebug() << "ButtonHandler: device opened successfully -" << m_devicePath;
    return true;
#else
    // Linux 이외 환경(개발 PC 등)에서는 키보드를 물리 버튼 대용으로 사용
    // MainWindow::keyPressEvent 에서 Space/Enter 를 buttonPressed()로 연결
    qWarning() << "ButtonHandler: non-Linux environment - keyboard fallback mode (Space/Enter)";
    return true;    // 에러 없이 정상 반환
#endif
}

void ButtonHandler::close()
{
#ifdef Q_OS_LINUX
    if (m_pollTimer) {
        m_pollTimer->stop();
        m_pollTimer = nullptr;
    }
    if (m_fd >= 0) {
        ::close(m_fd);
        m_fd = -1;
    }
#endif
}

bool ButtonHandler::isOpen() const
{
#ifdef Q_OS_LINUX
    return m_fd >= 0;
#else
    return true;
#endif
}

void ButtonHandler::setDebounceMs(int ms)
{
    m_debounceMs = ms;
}

int ButtonHandler::debounceMs() const
{
    return m_debounceMs;
}

void ButtonHandler::pollDevice()
{
#ifdef Q_OS_LINUX
    char buf[512] = {};
    ssize_t n = ::read(m_fd, buf, sizeof(buf) - 1);
    if (n <= 0) return;

    buf[n] = '\0';
    QString line = QString(buf);

    // Match both "key_isr:42" and "key_isr(): count = 42"
    QRegularExpression re(R"(key_isr[^0-9]*(\d+))");
    auto match = re.match(line);
    if (!match.hasMatch()) return;

    int currentCount = match.captured(1).toInt();
    qDebug() << "ButtonHandler: key_isr count =" << currentCount;

    if (m_lastCount < 0) {
        m_lastCount = currentCount;
        return;
    }

    if (currentCount > m_lastCount) {
        m_lastCount = currentCount;
        emit buttonPressed();
    }
#endif
}
