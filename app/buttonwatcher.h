#ifndef BUTTONWATCHER_H
#define BUTTONWATCHER_H

#include <QObject>
#include <QThread>

// Runs in a dedicated thread. Does a blocking read() on the device fd.
// Uses a self-pipe so that stop() can interrupt the blocking select() instantly.
class ButtonWorker : public QObject
{
    Q_OBJECT
public:
    explicit ButtonWorker(int fd, int stopPipeRead, QObject *parent = nullptr);

public slots:
    void run();

signals:
    void buttonPressed();

private:
    int m_fd;           // /dev/my_dev0 (blocking)
    int m_stopPipeRead; // read end of stop-pipe
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
    int          m_stopPipe[2]; // [0]=read, [1]=write
    QThread      m_thread;
    ButtonWorker *m_worker;
};

#endif // BUTTONWATCHER_H
