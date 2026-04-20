#ifndef ULTRASONICWATCHER_H
#define ULTRASONICWATCHER_H

#include <QObject>
#include <QThread>
#include <QVector>

// Runs in a dedicated thread. Periodically opens /dev/hcsr04_array, reads
// all 4 sensor distances, and emits distancesRead(). A stop-pipe allows
// clean cancellation without a busy-wait.
class UltrasonicWorker : public QObject
{
    Q_OBJECT
public:
    explicit UltrasonicWorker(int stopPipeRead, QObject *parent = nullptr);

public slots:
    void run();

signals:
    // distances[i] = distance in cm for sensor i (0-3); -1 means error/timeout
    void distancesRead(QVector<int> distances);

private:
    int m_stopPipeRead;
};

// Lives in the main thread. Owns the worker thread and relays its signal.
class UltrasonicWatcher : public QObject
{
    Q_OBJECT
public:
    explicit UltrasonicWatcher(QObject *parent = nullptr);
    ~UltrasonicWatcher() override;

signals:
    void distancesRead(QVector<int> distances);

private:
    int               m_stopPipe[2]; // [0]=read end, [1]=write end
    QThread           m_thread;
    UltrasonicWorker *m_worker;
};

#endif // ULTRASONICWATCHER_H
