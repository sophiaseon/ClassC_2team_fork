#ifndef BUTTONWATCHER_H
#define BUTTONWATCHER_H

#include <QObject>
#include <QThread>

// Runs in a dedicated thread. Uses poll() on both the device fd and an eventfd
// so that stop() can unblock poll() instantly without relying on close().
class ButtonWorker : public QObject
{
    Q_OBJECT
public:
    explicit ButtonWorker(int fd, int efd, QObject *parent = nullptr);

public slots:
    void run();   // called when thread starts
    void stop();  // writes to eventfd → poll() wakes up → loop exits

signals:
    void buttonPressed();

private:
    int m_fd;   // /dev/mydev (blocking)
    int m_efd;  // eventfd used as stop signal
};

// Lives in the main thread. Owns the worker thread and relays its signal.
class ButtonWatcher : public QObject
{
    Q_OBJECT
public:
    explicit ButtonWatcher(QObject *parent = nullptr);
    ~ButtonWatcher() override;

signals:
    void buttonPressed();

private:
    int          m_fd;
    int          m_efd;
    QThread      m_thread;
    ButtonWorker *m_worker;
};

#endif // BUTTONWATCHER_H
