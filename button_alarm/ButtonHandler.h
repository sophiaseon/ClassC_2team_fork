//#ifndef BUTTONHANDLER_H
//#define BUTTONHANDLER_H

//#endif // BUTTONHANDLER_H

#pragma once

#include <QObject>
#include <QElapsedTimer>
#include <QTimer>

// -------------------------------------------------------
// ButtonHandler
//   - Polls /dev/input/event* via QTimer (every 10ms)
//   - Emits buttonPressed() after debounce (default 50ms)
// -------------------------------------------------------

class ButtonHandler : public QObject
{
    Q_OBJECT

public:
    explicit ButtonHandler(const QString &devicePath = "/dev/input/event0",
                           int debounceMs = 50,
                           QObject *parent = nullptr);
    ~ButtonHandler() override;

    bool open();
    void close();
    bool isOpen() const;

    void setDebounceMs(int ms);
    int  debounceMs() const;

signals:
    void buttonPressed();
    void deviceError(const QString &message);

private slots:
    void pollDevice();

private:
    QString       m_devicePath;
    int           m_fd { -1 };
    int           m_debounceMs { 50 };
    QTimer       *m_pollTimer { nullptr };
    QElapsedTimer m_debounceTimer;
    int           m_lastCount { -1 };  // last read count from driver
};
