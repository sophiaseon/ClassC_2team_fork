#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QFuture>
#include <atomic>
#include <pthread.h>
#include <QList>
#include <QMainWindow>

class ButtonWatcher;
class QTcpServer;

class QLabel;
class QListWidget;
class QProcess;
class QPushButton;
class QTimer;
class QWidget;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void updateCurrentTime();
    void openAddDialog();
    void openEditDialog();
    void deleteSelectedAlarm();
    void setDebugAlarmPlus5Sec();
    void openStatDialog();
    void openAlarmStatDialog(int alarmIndex);
    void openFriendStatDialog();

private:
    void showStyledAlert(const QString &title, const QString &message);

    struct AlarmEntry {
        int       alarmId     = 0;    // unique ID assigned at creation
        QDateTime dateTime;
        bool      enabled     = true;
        QString   soundFile   = QDir::homePath() + "/test_contents/test.wav";
        int       dismissMode = 0; // 0=Simple 1=Game 2=Button 3=Camera 4=Ultrasonic
        int       gameType    = 0; // 0=NumberOrder 1=ColorMemory
        int       repeatMask  = 0; // bit0=Sun ... bit6=Sat
        bool      useSpecificDate = false;
        QString   logFile;        // persisted log path, e.g. ~/capture/alarm_3.txt
    };

    void buildUi();
    void sortAlarmsByTime();
    void refreshAlarmList();
    void startBuzzerTetris();
    void stopBuzzer();
    void loadAlarmCounter();
    void saveAlarmCounter();
    void saveAlarms();
    void loadAlarms();
    void startAlarmServer();

    static QString alarmLogPath(int alarmId)
    { return QDir::homePath() + QString("/capture/alarm_%1.txt").arg(alarmId); }

    QWidget     *m_centralWidget;
    QTimer      *m_clockTimer;
    QLabel      *m_currentTimeLabel;

    QListWidget *m_alarmListWidget;
    QLabel      *m_alarmCountLabel;

    QPushButton *m_addButton;
    QPushButton *m_editButton;
    QPushButton *m_deleteButton;
    QPushButton *m_debugPlus5SecButton;
    QPushButton *m_statButton;
    QPushButton *m_friendButton;
    QPushButton *m_exitButton;

    QList<AlarmEntry> m_alarms;
    QProcess         *m_alarmPlayer;
    ButtonWatcher    *m_buttonWatcher;
    QTcpServer       *m_alarmServer;

    int              m_nextAlarmId = 1;
    int              m_buzzerFd = -1;
    QFuture<void>    m_buzzerFuture;
    std::atomic<pthread_t> m_buzzerTid{0};  // thread running the ioctl
    bool             m_alarmHandling = false;  // re-entrancy guard

    QElapsedTimer m_actionTimer;
};

#endif // MAINWINDOW_H
