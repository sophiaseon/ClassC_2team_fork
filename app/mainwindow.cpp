#include "mainwindow.h"
#include "alarmdialog.h"
#include "buttonwatcher.h"
#include "dismissdialog.h"
#include "statdialog.h"

#include <algorithm>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <pthread.h>
#include <QtConcurrent>

// Buzzer ioctl constants (matches my_ioctl.h)
#define MY_IOCTL_MAGIC    'M'
#define IOCTL_PLAY_TETRIS _IO(MY_IOCTL_MAGIC, 0)
#define IOCTL_STOP        _IO(MY_IOCTL_MAGIC, 3)
#define BUZZER_DEVICE     "/dev/mybuzzer"
#include <QComboBox>
#include <QDir>
#include <QHostAddress>
#include <QPointer>
#include <QDialog>
#include <QFile>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QInputDialog>
#include <QMessageBox>
#include <QProcess>
#include <QPushButton>
#include <QEventLoop>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryFile>
#include <QTextStream>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace {

int weekdayIndexFromDate(const QDate &date)
{
    return date.dayOfWeek() % 7; // Mon=1..Sun=7 -> Sun=0..Sat=6
}

QDateTime computeNextRepeatDateTime(const QDateTime &now, const QTime &time, int repeatMask)
{
    for (int offset = 0; offset <= 7; ++offset) {
        const QDate candidateDate = now.date().addDays(offset);
        const int idx = weekdayIndexFromDate(candidateDate);
        if ((repeatMask & (1 << idx)) == 0) continue;

        const QDateTime candidate(candidateDate, time);
        if (candidate > now) return candidate;
    }
    return QDateTime(now.date().addDays(1), time);
}

QString repeatMaskToText(int repeatMask)
{
    if (repeatMask == 0) return QString();

    const char *names[7] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
    QStringList parts;
    for (int i = 0; i < 7; ++i) {
        if (repeatMask & (1 << i)) parts << names[i];
    }
    return parts.join(",");
}

QString formatRemaining(qint64 secs)
{
    if (secs < 0) secs = 0;

    const qint64 days = secs / 86400;
    secs %= 86400;
    const qint64 hours = secs / 3600;
    secs %= 3600;
    const qint64 mins = secs / 60;
    const qint64 s = secs % 60;

    if (days > 0) {
        return QString("in %1d %2h %3m").arg(days).arg(hours).arg(mins);
    }
    return QString("in %1h %2m %3s").arg(hours).arg(mins).arg(s);
}

} // namespace

// ── MainWindow ───────────────────────────────────────────────────────────────
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_centralWidget(nullptr)
    , m_clockTimer(new QTimer(this))
    , m_currentTimeLabel(nullptr)
    , m_alarmListWidget(nullptr)
    , m_alarmCountLabel(nullptr)
    , m_addButton(nullptr)
    , m_editButton(nullptr)
    , m_deleteButton(nullptr)
    , m_debugPlus5SecButton(nullptr)
    , m_statButton(nullptr)
    , m_friendButton(nullptr)
    , m_exitButton(nullptr)
    , m_alarmPlayer(new QProcess(this))
    , m_buttonWatcher(new ButtonWatcher(this))
    , m_alarmServer(nullptr)
{
    // Open buzzer device lazily on first use (insmod may not have happened yet)
    // Install SIGUSR1 handler: empty (prevents process termination) but wakes
    // msleep_interruptible in the kernel so the buzzer thread exits quickly.
    ::signal(SIGUSR1, [](int){});
    buildUi();

    m_actionTimer.start();

    loadAlarmCounter();
    loadAlarms();

    connect(m_clockTimer,         &QTimer::timeout,     this, &MainWindow::updateCurrentTime);
    connect(m_exitButton,         &QPushButton::clicked, this, &MainWindow::close);
    connect(m_addButton,          &QPushButton::clicked, this, &MainWindow::openAddDialog);
    connect(m_editButton,         &QPushButton::clicked, this, &MainWindow::openEditDialog);
    connect(m_deleteButton,       &QPushButton::clicked, this, &MainWindow::deleteSelectedAlarm);
    connect(m_debugPlus5SecButton,&QPushButton::clicked, this, &MainWindow::setDebugAlarmPlus5Sec);
    connect(m_statButton,         &QPushButton::clicked, this, &MainWindow::openStatDialog);
    connect(m_friendButton,        &QPushButton::clicked, this, &MainWindow::openFriendStatDialog);

    startAlarmServer();

    m_clockTimer->start(1000);
    updateCurrentTime();
}

MainWindow::~MainWindow()
{
    if (m_buzzerFd >= 0) {
        ::ioctl(m_buzzerFd, IOCTL_STOP);
    }
    const pthread_t tid = m_buzzerTid.load();
    if (tid != 0) ::pthread_kill(tid, SIGUSR1);
    m_buzzerFuture.waitForFinished();  // safe to block here in destructor
    if (m_buzzerFd >= 0) {
        ::close(m_buzzerFd);
        m_buzzerFd = -1;
    }
}

void MainWindow::startBuzzerTetris()
{
    // Lazy open: try to open device if not already open
    if (m_buzzerFd < 0)
        m_buzzerFd = ::open(BUZZER_DEVICE, O_RDWR);
    if (m_buzzerFd < 0) return;   // device still not available

    if (m_buzzerFuture.isRunning()) return;  // already playing

    const int fd = m_buzzerFd;
    m_buzzerFuture = QtConcurrent::run([this, fd]() {
        m_buzzerTid.store(::pthread_self());
        ::ioctl(fd, IOCTL_PLAY_TETRIS);
        m_buzzerTid.store(pthread_t{});
    });
}

void MainWindow::stopBuzzer()
{
    if (m_buzzerFd >= 0) {
        // 1. Set g_stop=1 in kernel AND silence PWM immediately
        ::ioctl(m_buzzerFd, IOCTL_STOP);
    }
    // 2. Wake up the thread blocked in msleep_interruptible so it exits fast
    const pthread_t tid = m_buzzerTid.load();
    if (tid != 0) {
        ::pthread_kill(tid, SIGUSR1);
    }
    // Do NOT waitForFinished() here — main thread must stay responsive
}

// ── buildUi ──────────────────────────────────────────────────────────────────
void MainWindow::buildUi()
{
    setWindowTitle("Make Our Morning Successful");
    setFixedSize(900, 600);

    m_centralWidget = new QWidget(this);
    m_centralWidget->setStyleSheet("background: #0b0b0b;");
    setCentralWidget(m_centralWidget);

    QVBoxLayout *root = new QVBoxLayout(m_centralWidget);
    root->setContentsMargins(20, 14, 20, 14);
    root->setSpacing(10);

    // ── top bar ──
    QHBoxLayout *topBar = new QHBoxLayout();
    topBar->setSpacing(8);

    QLabel *titleLabel = new QLabel("Make Our Morning Successful", this);
    titleLabel->setStyleSheet(
        "QLabel { font-size: 22px; font-weight: 700; color: white; }"
    );

    m_currentTimeLabel = new QLabel(this);
    m_currentTimeLabel->setAlignment(Qt::AlignCenter);
    m_currentTimeLabel->setStyleSheet(
        "QLabel {"
        "    font-size: 20px; font-weight: 700; color: white;"
        "    background: #202020; border-radius: 8px; padding: 4px 12px;"
        "}"
    );

    m_debugPlus5SecButton = new QPushButton("+5s", this);
    m_debugPlus5SecButton->setFixedSize(72, 40);
    m_debugPlus5SecButton->setStyleSheet(
        "QPushButton { font-size: 17px; font-weight: 700; color: white;"
        "    background: #3a7a3a; border: none; border-radius: 8px; }"
        "QPushButton:pressed { background: #2c5f2c; }"
    );

    m_statButton = new QPushButton("Stat", this);
    m_statButton->setFixedSize(72, 40);
    m_statButton->setStyleSheet(
        "QPushButton { font-size: 17px; font-weight: 700; color: white;"
        "    background: #7a5a3a; border: none; border-radius: 8px; }"
        "QPushButton:pressed { background: #5f4428; }"
    );

    m_friendButton = new QPushButton("Friend", this);
    m_friendButton->setFixedSize(80, 40);
    m_friendButton->setStyleSheet(
        "QPushButton { font-size: 17px; font-weight: 700; color: white;"
        "    background: #3a6a9a; border: none; border-radius: 8px; }"
        "QPushButton:pressed { background: #2a5077; }"
    );

    m_exitButton = new QPushButton("Exit", this);
    m_exitButton->setFixedSize(80, 40);
    m_exitButton->setStyleSheet(
        "QPushButton { font-size: 17px; font-weight: 700; color: white;"
        "    background: #cc3333; border: none; border-radius: 8px; }"
        "QPushButton:pressed { background: #992222; }"
    );

    topBar->addWidget(titleLabel);
    topBar->addStretch();
    topBar->addWidget(m_currentTimeLabel);
    topBar->addSpacing(12);
    topBar->addWidget(m_debugPlus5SecButton);
    topBar->addWidget(m_statButton);
    topBar->addWidget(m_friendButton);
    topBar->addWidget(m_exitButton);

    // ── alarm count label ──
    m_alarmCountLabel = new QLabel("No alarms", this);
    m_alarmCountLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_alarmCountLabel->setStyleSheet(
        "QLabel { font-size: 14px; color: #aaaaaa; padding: 0 2px; }"
    );

    // ── alarm list ──
    m_alarmListWidget = new QListWidget(this);
    m_alarmListWidget->setStyleSheet(
        "QListWidget {"
        "    background: #141414; border: 1px solid #3a3a3a;"
        "    border-radius: 10px; color: white; font-size: 17px;"
        "    padding: 6px; outline: none;"
        "}"
        "QListWidget::item { padding: 10px 14px; border-radius: 6px; }"
        "QListWidget::item:selected { background: #2d7dff; color: white; }"
        "QListWidget::item:hover:!selected { background: #222222; }"
    );

    // ── action buttons row ──
    QHBoxLayout *btnRow = new QHBoxLayout();
    btnRow->setSpacing(10);

    auto makeBtn = [this](const QString &label, const QString &bg, const QString &bgPressed) -> QPushButton * {
        QPushButton *btn = new QPushButton(label, this);
        btn->setFixedHeight(52);
        btn->setStyleSheet(
            QString(
                "QPushButton { font-size: 17px; font-weight: 700; color: white;"
                "    background: %1; border: none; border-radius: 10px; }"
                "QPushButton:pressed { background: %2; }"
            ).arg(bg, bgPressed)
        );
        return btn;
    };

    m_addButton    = makeBtn("+ Add Alarm", "#2d7dff", "#1d5fc7");
    m_editButton   = makeBtn("Edit",        "#555555", "#333333");
    m_deleteButton = makeBtn("Delete",      "#993333", "#771111");

    btnRow->addWidget(m_addButton,    2);
    btnRow->addWidget(m_editButton,   1);
    btnRow->addWidget(m_deleteButton, 1);

    root->addLayout(topBar);
    root->addWidget(m_alarmCountLabel);
    root->addWidget(m_alarmListWidget, 1);
    root->addLayout(btnRow);
}

void MainWindow::sortAlarmsByTime()
{
    std::sort(m_alarms.begin(), m_alarms.end(),
              [](const AlarmEntry &a, const AlarmEntry &b) {
                  return a.dateTime < b.dateTime;
              });
}

// ── refreshAlarmList ──────────────────────────────────────────────────────────
void MainWindow::refreshAlarmList()
{
    sortAlarmsByTime();

    const int currentRow = m_alarmListWidget->currentRow();
    m_alarmListWidget->clear();
    const QDateTime now = QDateTime::currentDateTime();

    int activeCount = 0;
    for (int i = 0; i < m_alarms.size(); ++i) {
        const AlarmEntry &e = m_alarms.at(i);
        QString mainText = QString("%1   ").arg(i + 1)
                         + e.dateTime.toString("yyyy-MM-dd  hh:mm");
        const QString repeatText = repeatMaskToText(e.repeatMask);
        if (!repeatText.isEmpty()) {
            mainText += "   [" + repeatText + "]";
        }

        QString subText;
        if (e.enabled) {
            subText = formatRemaining(now.secsTo(e.dateTime));
        } else {
            subText = "disabled";
        }

        QWidget *rowWidget = new QWidget(m_alarmListWidget);
        rowWidget->setStyleSheet("background: transparent;");
        QHBoxLayout *rowLayout = new QHBoxLayout(rowWidget);
        rowLayout->setContentsMargins(8, 4, 8, 4);
        rowLayout->setSpacing(8);

        QWidget *textWrap = new QWidget(rowWidget);
        QVBoxLayout *textLayout = new QVBoxLayout(textWrap);
        textLayout->setContentsMargins(0, 0, 0, 0);
        textLayout->setSpacing(1);

        QLabel *mainLbl = new QLabel(mainText, textWrap);
        mainLbl->setStyleSheet(
            QString("QLabel { font-size: 16px; color: %1; }")
                .arg(e.enabled ? "#ffffff" : "#777777")
        );
        QLabel *subLbl = new QLabel(subText, textWrap);
        subLbl->setStyleSheet(
            QString("QLabel { font-size: 12px; color: %1; }")
                .arg(e.enabled ? "#66cc66" : "#888888")
        );

        textLayout->addWidget(mainLbl);
        textLayout->addWidget(subLbl);

        QPushButton *enableBtn = new QPushButton(rowWidget);
        enableBtn->setFixedSize(82, 34);
        if (e.enabled) {
            enableBtn->setText("Disable");
            enableBtn->setStyleSheet(
                "QPushButton { font-size: 17px; font-weight: 700; color: white;"
                "    background: #8b3a3a; border: none; border-radius: 8px; }"
                "QPushButton:pressed { background: #6a2a2a; }"
            );
            connect(enableBtn, &QPushButton::clicked, this, [this, i]() {
                if (i < 0 || i >= m_alarms.size()) return;
                m_alarms[i].enabled = false;
                saveAlarms();
                refreshAlarmList();
            });
        } else {
            enableBtn->setText("Enable");
            enableBtn->setStyleSheet(
                "QPushButton { font-size: 17px; font-weight: 700; color: white;"
                "    background: #2d7dff; border: none; border-radius: 8px; }"
                "QPushButton:pressed { background: #1d5fc7; }"
            );
            connect(enableBtn, &QPushButton::clicked, this, [this, i]() {
                if (i < 0 || i >= m_alarms.size()) return;

                AlarmEntry &entry = m_alarms[i];
                if (entry.enabled) return;

                entry.enabled = true;
                const QDateTime now2 = QDateTime::currentDateTime();
                if (entry.repeatMask != 0) {
                    entry.dateTime = computeNextRepeatDateTime(now2, entry.dateTime.time(), entry.repeatMask);
                } else {
                    QDate d = now2.date();
                    if (entry.dateTime.time() <= now2.time()) d = d.addDays(1);
                    entry.dateTime = QDateTime(d, entry.dateTime.time());
                    entry.useSpecificDate = false;
                }

                saveAlarms();
                refreshAlarmList();
            });
        }

        rowLayout->addWidget(textWrap, 1);
        rowLayout->addWidget(enableBtn, 0, Qt::AlignVCenter);

        QPushButton *statBtn = new QPushButton("Stat", rowWidget);
        statBtn->setFixedSize(52, 34);
        statBtn->setStyleSheet(
            "QPushButton { font-size: 17px; font-weight: 700; color: white;"
            "    background: #7a5a3a; border: none; border-radius: 8px; }"
            "QPushButton:pressed { background: #5f4428; }"
        );
        connect(statBtn, &QPushButton::clicked, this, [this, i, statBtn]() {
            QPointer<QPushButton> safe = statBtn;
            if (!safe) return;
            safe->setEnabled(false);
            QTimer::singleShot(500, this, [safe]() { if (safe) safe->setEnabled(true); });
            openAlarmStatDialog(i);
        });
        rowLayout->addWidget(statBtn, 0, Qt::AlignVCenter);

        QListWidgetItem *item = new QListWidgetItem(m_alarmListWidget);
        item->setSizeHint(QSize(0, 58));
        m_alarmListWidget->addItem(item);
        m_alarmListWidget->setItemWidget(item, rowWidget);

        if (e.enabled) ++activeCount;
    }

    if (m_alarms.isEmpty()) {
        m_alarmCountLabel->setText("No alarms");
    } else {
        m_alarmCountLabel->setText(
            QString("%1 alarm(s)  —  %2 active").arg(m_alarms.size()).arg(activeCount)
        );
    }

    // Restore selection if possible
    if (currentRow >= 0 && currentRow < m_alarmListWidget->count()) {
        m_alarmListWidget->setCurrentRow(currentRow);
    }
}

// ── updateCurrentTime ─────────────────────────────────────────────────────────
void MainWindow::updateCurrentTime()
{
    const QDateTime now = QDateTime::currentDateTime();
    m_currentTimeLabel->setText(now.toString("yyyy-MM-dd  hh:mm:ss"));
    refreshAlarmList();

    // Collect due alarm info
    struct TriggeredInfo {
        int index;
        int alarmId;
        QString alarmTime;
        QString soundFile;
        int dismissMode;
        int gameType;
    };
    QList<TriggeredInfo> triggered;

    for (int i = 0; i < m_alarms.size(); ++i) {
        const AlarmEntry &entry = m_alarms.at(i);
        if (entry.enabled && now >= entry.dateTime) {
            triggered.append({ i,
                               entry.alarmId,
                               entry.dateTime.toString("yyyy-MM-dd hh:mm"),
                               entry.soundFile,
                               entry.dismissMode,
                               entry.gameType });
        }
    }

    if (triggered.isEmpty()) return;

    // Prevent re-entrant alarm handling: dlg.exec() runs a nested event loop
    // which would re-fire the 1-second timer, stacking multiple DismissDialogs.
    if (m_alarmHandling) return;
    m_alarmHandling = true;

    // Play sound — loop until dialog is dismissed via polling timer
    const QString &soundFile = triggered.first().soundFile;
    QTimer *loopTimer = nullptr;
    if (soundFile == "buzzer:tetris") {
        startBuzzerTetris();
    } else {
        // Poll every 200ms: if aplay finished and alarm still active, restart it.
        // More reliable than QProcess::finished signal across Qt versions.
        loopTimer = new QTimer(this);
        loopTimer->setInterval(200);
        connect(loopTimer, &QTimer::timeout, this, [this, soundFile]() {
            if (m_alarmPlayer->state() == QProcess::NotRunning)
                m_alarmPlayer->start("aplay", QStringList() << "-D" << "hw:3,0" << soundFile);
        });
        m_alarmPlayer->start("aplay", QStringList() << "-D" << "hw:3,0" << soundFile);
        loopTimer->start();
    }

    // Collect alarm times and determine mode priority: Camera > Button > Ultrasonic > Game > Simple
    QStringList alarmTimes;
    bool needGame       = false;
    bool needButton     = false;
    bool needCamera     = false;
    bool needUltrasonic = false;
    DismissDialog::GameType selectedGameType = DismissDialog::NumberOrder;
    for (const TriggeredInfo &t : triggered) {
        alarmTimes << t.alarmTime;
        if (t.dismissMode == AlarmDialog::DismissGame) {
            needGame = true;
            selectedGameType = (t.gameType == AlarmDialog::GameColorMemory)
                ? DismissDialog::ColorMemory
                : DismissDialog::NumberOrder;
        }
        if (t.dismissMode == AlarmDialog::DismissButton)     needButton     = true;
        if (t.dismissMode == AlarmDialog::DismissCamera)     needCamera     = true;
        if (t.dismissMode == AlarmDialog::DismissUltrasonic) needUltrasonic = true;
    }

    DismissDialog::Mode dlgMode = DismissDialog::Simple;
    if (needCamera)         dlgMode = DismissDialog::Camera;
    else if (needButton)    dlgMode = DismissDialog::Button;
    else if (needUltrasonic) dlgMode = DismissDialog::Ultrasonic;
    else if (needGame)      dlgMode = DismissDialog::Game;

    // Show appropriate dismiss dialog
    int dlgAlarmId = -1;
    if (dlgMode == DismissDialog::Camera) {
        for (const TriggeredInfo &t : triggered) {
            if (t.dismissMode == AlarmDialog::DismissCamera) {
                dlgAlarmId = t.alarmId;
                break;
            }
        }
    }
    DismissDialog dlg(alarmTimes, dlgMode, selectedGameType, this, dlgAlarmId);
    if (dlgMode == DismissDialog::Button) {
        connect(m_buttonWatcher, &ButtonWatcher::buttonPressed,
                &dlg, &DismissDialog::onButtonPressedForGame);
    } else if (dlgMode == DismissDialog::Camera) {
        connect(m_buttonWatcher, &ButtonWatcher::buttonPressed,
                &dlg, &DismissDialog::captureByButton);
    }
    dlg.exec();

    // Stop loop timer before killing aplay
    if (loopTimer) {
        loopTimer->stop();
        loopTimer->deleteLater();
    }

    const QString capturedPhotoPath = dlg.capturedPhotoPath();

    // Write dismiss record to log
    {
        const QString dismissTime =
            QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
        QDir().mkpath(QDir::homePath() + "/capture");
        QFile logFile(QDir::homePath() + "/alarm.txt");
        if (logFile.open(QIODevice::Append | QIODevice::Text)) {
            QTextStream out(&logFile);
            for (const TriggeredInfo &t : triggered) {
                out << "ALARM: " << t.alarmTime
                    << " | DISMISSED: " << dismissTime;
                if (t.dismissMode == AlarmDialog::DismissCamera && !capturedPhotoPath.isEmpty()) {
                    out << " | PHOTO: " << capturedPhotoPath;
                }
                out << "\n";
            }
            logFile.close();
        }
        // Write per-alarm log files
        for (const TriggeredInfo &t : triggered) {
            if (t.index < 0 || t.index >= m_alarms.size()) continue;
            const QString &path = m_alarms[t.index].logFile;
            if (path.isEmpty()) continue;
            QFile alarmLog(path);
            if (alarmLog.open(QIODevice::Append | QIODevice::Text)) {
                QTextStream out(&alarmLog);
                out << "ALARM: " << t.alarmTime
                    << " | DISMISSED: " << dismissTime;
                if (t.dismissMode == AlarmDialog::DismissCamera && !capturedPhotoPath.isEmpty()) {
                    out << " | PHOTO: " << capturedPhotoPath;
                }
                out << "\n";
                alarmLog.close();
            }
        }
    }

    // Update alarms after dismiss
    const QDateTime postDismissNow = QDateTime::currentDateTime();
    for (const TriggeredInfo &t : triggered) {
        if (t.index < 0 || t.index >= m_alarms.size()) continue;

        AlarmEntry &e = m_alarms[t.index];
        if (e.repeatMask != 0) {
            e.enabled = true;
            e.useSpecificDate = false;
            e.dateTime = computeNextRepeatDateTime(postDismissNow, e.dateTime.time(), e.repeatMask);
        } else {
            e.enabled = false;
        }
    }

    saveAlarms();
    refreshAlarmList();

    // Stop player / buzzer
    m_alarmPlayer->terminate();
    m_alarmPlayer->waitForFinished(500);
    if (m_alarmPlayer->state() != QProcess::NotRunning) {
        m_alarmPlayer->kill();
    }
    stopBuzzer();
    m_alarmHandling = false;
}

// ── openAddDialog ─────────────────────────────────────────────────────────────
void MainWindow::openAddDialog()
{
    m_addButton->setEnabled(false);
    QTimer::singleShot(500, this, [this]() { m_addButton->setEnabled(true); });

    AlarmDialog dlg(this);
    if (dlg.exec() != QDialog::Accepted) return;

    AlarmEntry entry;
    entry.alarmId     = m_nextAlarmId++;
    entry.dateTime    = dlg.selectedDateTime();
    entry.enabled     = true;
    entry.soundFile   = dlg.soundFile();
    entry.dismissMode = dlg.dismissMode();
    entry.gameType    = dlg.gameType();
    entry.repeatMask  = dlg.repeatMask();
    entry.useSpecificDate = dlg.useSpecificDate();
    entry.logFile     = alarmLogPath(entry.alarmId);
    m_alarms.append(entry);
    saveAlarmCounter();
    saveAlarms();
    sortAlarmsByTime();
    refreshAlarmList();
    for (int i = 0; i < m_alarms.size(); ++i) {
        if (m_alarms[i].dateTime == entry.dateTime &&
            m_alarms[i].enabled == entry.enabled &&
            m_alarms[i].dismissMode == entry.dismissMode &&
            m_alarms[i].gameType == entry.gameType &&
            m_alarms[i].repeatMask == entry.repeatMask) {
            m_alarmListWidget->setCurrentRow(i);
            break;
        }
    }
}

// ── showStyledAlert ───────────────────────────────────────────────────────────
void MainWindow::showStyledAlert(const QString &title, const QString &message)
{
    QDialog dlg(this);
    dlg.setWindowTitle(title);
    dlg.setFixedSize(420, 200);
    dlg.setStyleSheet(
        "QDialog {"
        "    background: #0b0b0b;"
        "    border: 2px solid #ffffff;"
        "    border-radius: 4px;"
        "}"
    );

    QVBoxLayout *root = new QVBoxLayout(&dlg);
    root->setContentsMargins(28, 24, 28, 20);
    root->setSpacing(0);

    QLabel *titleLbl = new QLabel(title, &dlg);
    titleLbl->setStyleSheet(
        "QLabel { font-size: 18px; font-weight: 700; color: #ffffff; }"
    );
    root->addWidget(titleLbl);
    root->addSpacing(12);

    QLabel *msgLbl = new QLabel(message, &dlg);
    msgLbl->setWordWrap(true);
    msgLbl->setStyleSheet(
        "QLabel { font-size: 15px; color: #aaaaaa; }"
    );
    root->addWidget(msgLbl);
    root->addStretch();

    QPushButton *okBtn = new QPushButton("OK", &dlg);
    okBtn->setFixedHeight(44);
    okBtn->setStyleSheet(
        "QPushButton {"
        "    font-size: 17px; font-weight: 700; color: white;"
        "    background: #2d7dff; border: none; border-radius: 8px;"
        "}"
        "QPushButton:pressed { background: #1d5fc7; }"
    );
    connect(okBtn, &QPushButton::clicked, &dlg, &QDialog::accept);
    root->addWidget(okBtn);

    dlg.exec();
}

// ── openEditDialog ────────────────────────────────────────────────────────────
void MainWindow::openEditDialog()
{
    m_editButton->setEnabled(false);
    QTimer::singleShot(500, this, [this]() { m_editButton->setEnabled(true); });

    const int row = m_alarmListWidget->currentRow();
    if (row < 0 || row >= m_alarms.size()) {
        showStyledAlert("No Alarm Selected", "Please select an alarm from the list to edit.");
        return;
    }

    AlarmDialog dlg(this, row, m_alarms.at(row).dateTime,
                     m_alarms.at(row).soundFile, m_alarms.at(row).dismissMode,
                     m_alarms.at(row).gameType,
                     m_alarms.at(row).repeatMask,
                     m_alarms.at(row).useSpecificDate);
    if (dlg.exec() != QDialog::Accepted) return;

    m_alarms[row].dateTime    = dlg.selectedDateTime();
    m_alarms[row].enabled     = true;
    m_alarms[row].soundFile   = dlg.soundFile();
    m_alarms[row].dismissMode = dlg.dismissMode();
    m_alarms[row].gameType    = dlg.gameType();
    m_alarms[row].repeatMask  = dlg.repeatMask();
    m_alarms[row].useSpecificDate = dlg.useSpecificDate();
    sortAlarmsByTime();
    saveAlarms();
    refreshAlarmList();
}

// ── deleteSelectedAlarm ───────────────────────────────────────────────────────
void MainWindow::deleteSelectedAlarm()
{
    m_deleteButton->setEnabled(false);
    QTimer::singleShot(500, this, [this]() { m_deleteButton->setEnabled(true); });

    const int row = m_alarmListWidget->currentRow();
    if (row < 0 || row >= m_alarms.size()) return;
    m_alarms.removeAt(row);
    saveAlarms();
    refreshAlarmList();
}

// ── setDebugAlarmPlus5Sec ─────────────────────────────────────────────────────
void MainWindow::setDebugAlarmPlus5Sec()
{
    m_debugPlus5SecButton->setEnabled(false);
    QTimer::singleShot(500, this, [this]() { m_debugPlus5SecButton->setEnabled(true); });

    QDialog dlg(this);
    dlg.setWindowTitle("Debug +5s Alarm");
    dlg.setFixedSize(520, 280);
    dlg.setStyleSheet(
        "QDialog { background: #0b0b0b; }"
        "QLabel { color: #dddddd; }"
        "QComboBox {"
        "    font-size: 14px; color: white; background: #202020;"
        "    border: 1px solid #4a4a4a; border-radius: 8px; padding: 4px 10px;"
        "}"
        "QComboBox::drop-down { border: none; width: 24px; }"
        "QComboBox QAbstractItemView {"
        "    background: #202020; color: white; selection-background-color: #2d7dff;"
        "}"
        "QPushButton {"
        "    font-size: 15px; font-weight: 700; color: white;"
        "    background: #3f3f3f; border: none; border-radius: 9px;"
        "}"
        "QPushButton:pressed { background: #2e2e2e; }"
    );

    QVBoxLayout *root = new QVBoxLayout(&dlg);
    root->setContentsMargins(20, 16, 20, 16);
    root->setSpacing(10);

    QLabel *info = new QLabel(
        "Alarm will be set to 5 seconds from when you click \"Add +5s Alarm\".",
        &dlg);
    info->setStyleSheet("QLabel { font-size: 14px; color: #dddddd; }");
    info->setWordWrap(true);
    root->addWidget(info);

    QLabel *soundLabel = new QLabel("Alarm Sound", &dlg);
    soundLabel->setStyleSheet("QLabel { font-size: 13px; color: #aaaaaa; }");
    QComboBox *soundCombo = new QComboBox(&dlg);
    soundCombo->addItem("test.wav",  QDir::homePath() + "/test_contents/test.wav");
    soundCombo->addItem("test2.wav", QDir::homePath() + "/test_contents/test2.wav");
    soundCombo->setFixedHeight(40);
    root->addWidget(soundLabel);
    root->addWidget(soundCombo);

    QLabel *modeLabel = new QLabel("Dismiss Mode", &dlg);
    modeLabel->setStyleSheet("QLabel { font-size: 13px; color: #aaaaaa; }");
    QComboBox *modeCombo = new QComboBox(&dlg);
    modeCombo->addItem("Simple Dismiss", AlarmDialog::DismissSimple);
    modeCombo->addItem("Game Mode", AlarmDialog::DismissGame);
    modeCombo->addItem("Physical Button", AlarmDialog::DismissButton);
    modeCombo->addItem("Camera", AlarmDialog::DismissCamera);
    modeCombo->setFixedHeight(40);
    root->addWidget(modeLabel);
    root->addWidget(modeCombo);

    QComboBox *gameCombo = new QComboBox(&dlg);
    gameCombo->addItem("Number Order (1-25)", AlarmDialog::GameNumberOrder);
    gameCombo->addItem("Color Memory (5-6-7)", AlarmDialog::GameColorMemory);
    gameCombo->setFixedHeight(38);
    root->addWidget(gameCombo);

    auto refreshGameVisibility = [modeCombo, gameCombo]() {
        const int mode = modeCombo->currentData().toInt();
        const bool showGame = (mode == AlarmDialog::DismissGame);
        gameCombo->setVisible(showGame);
    };
    connect(soundCombo, QOverload<int>::of(&QComboBox::activated), &dlg, [soundCombo](int) {
        soundCombo->setEnabled(false);
        QTimer::singleShot(300, soundCombo, [soundCombo]() { soundCombo->setEnabled(true); });
    });
    connect(modeCombo, QOverload<int>::of(&QComboBox::activated), &dlg,
            [modeCombo, gameCombo, refreshGameVisibility](int) {
                refreshGameVisibility();
                modeCombo->setEnabled(false);
                QTimer::singleShot(300, modeCombo, [modeCombo]() { modeCombo->setEnabled(true); });
            });
    connect(gameCombo, QOverload<int>::of(&QComboBox::activated), &dlg, [gameCombo](int) {
        gameCombo->setEnabled(false);
        QTimer::singleShot(300, gameCombo, [gameCombo]() { gameCombo->setEnabled(true); });
    });
    refreshGameVisibility();

    root->addStretch();

    QHBoxLayout *btnRow = new QHBoxLayout();
    QPushButton *addBtn = new QPushButton("Add +5s Alarm", &dlg);
    QPushButton *cancelBtn = new QPushButton("Cancel", &dlg);
    addBtn->setFixedHeight(42);
    cancelBtn->setFixedHeight(42);
    addBtn->setStyleSheet(
        "QPushButton { font-size: 15px; font-weight: 700; color: white;"
        "    background: #2d7dff; border: none; border-radius: 9px; }"
        "QPushButton:pressed { background: #1d5fc7; }"
    );
    connect(addBtn, &QPushButton::clicked, &dlg, &QDialog::accept);
    connect(cancelBtn, &QPushButton::clicked, &dlg, &QDialog::reject);
    btnRow->addWidget(addBtn, 1);
    btnRow->addWidget(cancelBtn, 1);
    root->addLayout(btnRow);

    if (dlg.exec() != QDialog::Accepted) return;

    const QDateTime target = QDateTime::currentDateTime().addSecs(5);

    AlarmEntry entry;
    entry.alarmId   = m_nextAlarmId++;
    entry.dateTime  = target;
    entry.enabled = true;
    entry.soundFile = soundCombo->currentData().toString();
    entry.dismissMode = modeCombo->currentData().toInt();
    entry.gameType = gameCombo->currentData().toInt();
    entry.repeatMask = 0;
    entry.useSpecificDate = false;
    entry.logFile   = alarmLogPath(entry.alarmId);
    m_alarms.append(entry);
    saveAlarmCounter();
    saveAlarms();
    sortAlarmsByTime();
    refreshAlarmList();
}

// ── openStatDialog ────────────────────────────────────────────────────────────
void MainWindow::openStatDialog()
{
    m_statButton->setEnabled(false);
    QTimer::singleShot(500, this, [this]() { m_statButton->setEnabled(true); });

    StatDialog dlg(QDir::homePath() + "/alarm.txt", this);
    dlg.exec();
}
// ── openFriendStatDialog ───────────────────────────────────────────
void MainWindow::openFriendStatDialog()
{
    static const quint16 ALARM_PORT = 45678;
    static const int TIMEOUT_MS = 5000;
    const QString ipFilePath = QDir::homePath() + "/friend_ip.txt";

    // Load saved IP, or prompt the user to enter one
    QString friendIp;
    QFile ipFile(ipFilePath);
    if (ipFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        friendIp = QString::fromUtf8(ipFile.readAll()).trimmed();
        ipFile.close();
    }

    if (friendIp.isEmpty()) {
        // ── On-screen IP keypad dialog ────────────────────────────────────
        QDialog ipDlg(this);
        ipDlg.setWindowTitle("Friend's IP");
        ipDlg.setFixedSize(420, 420);
        ipDlg.setStyleSheet(
            "QDialog { background: #0b0b0b; border: 2px solid #ffffff; border-radius: 4px; }"
        );

        QVBoxLayout *dlgRoot = new QVBoxLayout(&ipDlg);
        dlgRoot->setContentsMargins(20, 16, 20, 16);
        dlgRoot->setSpacing(12);

        QLabel *hint = new QLabel("Enter friend's IP address", &ipDlg);
        hint->setAlignment(Qt::AlignCenter);
        hint->setStyleSheet("QLabel { font-size: 15px; color: #aaaaaa; }");
        dlgRoot->addWidget(hint);

        QLabel *display = new QLabel("", &ipDlg);
        display->setAlignment(Qt::AlignCenter);
        display->setFixedHeight(52);
        display->setStyleSheet(
            "QLabel { font-size: 24px; font-weight: 700; color: white;"
            "    background: #1e1e1e; border: 1px solid #3a3a3a;"
            "    border-radius: 8px; padding: 0 12px; }"
        );
        dlgRoot->addWidget(display);

        // Keypad: 1-9, ., 0, backspace
        QGridLayout *grid = new QGridLayout();
        grid->setSpacing(8);

        auto makeKey = [&](const QString &label, const QString &bg, const QString &bgPress) -> QPushButton * {
            QPushButton *btn = new QPushButton(label, &ipDlg);
            btn->setFixedSize(88, 60);
            btn->setStyleSheet(
                QString("QPushButton { font-size: 20px; font-weight: 700; color: white;"
                        "    background: %1; border: none; border-radius: 8px; }"
                        "QPushButton:pressed { background: %2; }").arg(bg, bgPress)
            );
            return btn;
        };

        const struct { QString lbl; int row; int col; } keys[] = {
            {"1",0,0},{"2",0,1},{"3",0,2},
            {"4",1,0},{"5",1,1},{"6",1,2},
            {"7",2,0},{"8",2,1},{"9",2,2},
            {".",3,0},{"0",3,1},
        };
        for (const auto &k : keys) {
            QPushButton *btn = makeKey(k.lbl, "#2a2a2a", "#444444");
            grid->addWidget(btn, k.row, k.col);
            const QString ch = k.lbl;
            connect(btn, &QPushButton::clicked, &ipDlg, [display, ch]() {
                if (display->text().length() < 15)
                    display->setText(display->text() + ch);
            });
        }

        QPushButton *bksp = makeKey("<", "#5a3a3a", "#7a2a2a");
        grid->addWidget(bksp, 3, 2);
        connect(bksp, &QPushButton::clicked, &ipDlg, [display]() {
            const QString t = display->text();
            if (!t.isEmpty()) display->setText(t.left(t.length() - 1));
        });

        dlgRoot->addLayout(grid);

        QHBoxLayout *btnRow = new QHBoxLayout();
        btnRow->setSpacing(12);
        QPushButton *cancelBtn = makeKey("Cancel", "#cc3333", "#992222");
        cancelBtn->setFixedWidth(160);
        QPushButton *okBtn     = makeKey("OK",     "#3a6a9a", "#2a5077");
        okBtn->setFixedWidth(160);
        btnRow->addWidget(cancelBtn);
        btnRow->addWidget(okBtn);
        dlgRoot->addLayout(btnRow);

        connect(cancelBtn, &QPushButton::clicked, &ipDlg, &QDialog::reject);
        connect(okBtn,     &QPushButton::clicked, &ipDlg, [&]() {
            if (!display->text().trimmed().isEmpty()) ipDlg.accept();
        });

        if (ipDlg.exec() != QDialog::Accepted) return;
        friendIp = display->text().trimmed();
        if (friendIp.isEmpty()) return;

        // Persist for next time
        QFile out(ipFilePath);
        if (out.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate))
            out.write(friendIp.toUtf8());
    }

    m_friendButton->setEnabled(false);

    // Use a local QEventLoop so the GUI event loop keeps running during the
    // entire network transaction.  This guarantees our own QTcpServer can
    // still fire newConnection while we are waiting for the friend's reply.
    QTcpSocket socket;
    QEventLoop  loop;
    QTimer      timer;
    timer.setSingleShot(true);
    bool connected = false;

    // ── Connection phase ──────────────────────────────────────────────
    connect(&socket, &QTcpSocket::connected,     this, [&]() { connected = true; loop.quit(); });
    connect(&socket, &QTcpSocket::disconnected,  &loop, &QEventLoop::quit);
    connect(&timer,  &QTimer::timeout,           &loop, &QEventLoop::quit);
    timer.start(TIMEOUT_MS);
    socket.connectToHost(friendIp, ALARM_PORT);
    loop.exec();
    timer.stop();

    m_friendButton->setEnabled(true);
    if (!connected) {
        showStyledAlert("Friend Stat",
            QString("Cannot connect to friend's board.\n(%1)").arg(socket.errorString()));
        return;
    }

    // Ask the server for the alarm log
    socket.write("GET_LOG\n");

    // ── Receive phase ─────────────────────────────────────────────────
    // Grab any bytes already in the buffer, then wait for the server to
    // close the connection (which signals "all data sent").
    QByteArray data;
    connect(&socket, &QTcpSocket::readyRead, this, [&]() { data += socket.readAll(); });
    // disconnected already wired to loop.quit() above
    timer.start(TIMEOUT_MS);
    data += socket.readAll(); // drain anything buffered before exec()
    if (socket.state() != QAbstractSocket::UnconnectedState)
        loop.exec();
    timer.stop();
    data += socket.readAll(); // final drain
    socket.disconnectFromHost();

    if (data.isEmpty()) {
        showStyledAlert("Friend Stat", "No data received from friend's board.");
        return;
    }

    StatDialog dlg(data, friendIp, this,
        QString("Friend's Alarm Statistics (%1)").arg(friendIp));
    dlg.exec();
}

// ── startAlarmServer ────────────────────────────────────────────────
void MainWindow::startAlarmServer()
{
    static const quint16 ALARM_PORT = 45678;

    m_alarmServer = new QTcpServer(this);
    if (!m_alarmServer->listen(QHostAddress::Any, ALARM_PORT)) {
        qWarning() << "[AlarmServer] Failed to listen on port" << ALARM_PORT
                   << ":" << m_alarmServer->errorString();
        return;
    }
    qDebug() << "[AlarmServer] Listening on port" << ALARM_PORT;

    connect(m_alarmServer, &QTcpServer::newConnection, this, [this]() {
        QTcpSocket *sock = m_alarmServer->nextPendingConnection();
        if (!sock) return;

        // Non-blocking: wait for the client's command line via signal.
        connect(sock, &QTcpSocket::readyRead, this, [this, sock]() {
            if (!sock->canReadLine())
                return;

            const QString command = QString::fromUtf8(sock->readLine()).trimmed();

            QByteArray payload;
            if (command == "GET_LOG" || command.isEmpty()) {
                QFile f(QDir::homePath() + "/alarm.txt");
                if (f.open(QIODevice::ReadOnly))
                    payload = f.readAll();
            } else if (command.startsWith("GET_PHOTO:")) {
                const QString path = command.mid(10).trimmed();
                const QString home = QDir::homePath();
                // Security: only serve files inside the home directory; reject traversal
                if (path.startsWith(home) && !path.contains("..")) {
                    QFile f(path);
                    if (f.open(QIODevice::ReadOnly))
                        payload = f.readAll();
                } else {
                    qWarning() << "[AlarmServer] Rejected path:" << path;
                }
            }

            sock->write(payload);
            // disconnectFromHost() flushes all pending writes before sending FIN
            sock->disconnectFromHost();
        });
        connect(sock, &QTcpSocket::disconnected, sock, &QObject::deleteLater);
    });
}
// ── openAlarmStatDialog ───────────────────────────────────────────────────────
void MainWindow::openAlarmStatDialog(int alarmIndex)
{
    if (alarmIndex < 0 || alarmIndex >= m_alarms.size()) return;
    const AlarmEntry &e = m_alarms.at(alarmIndex);
    const QString logPath = e.logFile.isEmpty() ? alarmLogPath(e.alarmId) : e.logFile;
    const QString repeatText = repeatMaskToText(e.repeatMask);
    const QString timeStr = e.dateTime.toString("hh:mm");
    const QString title = repeatText.isEmpty()
        ? QString("Alarm #%1 (%2) Statistics").arg(alarmIndex + 1).arg(timeStr)
        : QString("Alarm #%1 (%2  [%3]) Statistics")
              .arg(alarmIndex + 1).arg(timeStr).arg(repeatText);
    StatDialog dlg(logPath, this, title);
    dlg.exec();
}

// ── loadAlarmCounter / saveAlarmCounter ───────────────────────────────────────
#define ALARM_COUNTER_FILE (QDir::homePath() + "/capture/alarm_counter.txt")

void MainWindow::loadAlarmCounter()
{
    QDir().mkpath(QDir::homePath() + "/capture");
    QFile f(ALARM_COUNTER_FILE);
    if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&f);
        int saved = 0;
        in >> saved;
        f.close();
        if (saved > 0)
            m_nextAlarmId = saved;
    }
}

void MainWindow::saveAlarmCounter()
{
    QDir().mkpath(QDir::homePath() + "/capture");
    QFile f(ALARM_COUNTER_FILE);
    if (f.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        QTextStream out(&f);
        out << m_nextAlarmId;
        f.close();
    }
}

// ── saveAlarms / loadAlarms ───────────────────────────────────────────────────
#define ALARMS_SAVE_FILE (QDir::homePath() + "/capture/alarms_save.json")

void MainWindow::saveAlarms()
{
    QDir().mkpath(QDir::homePath() + "/capture");
    QJsonArray arr;
    for (const AlarmEntry &e : m_alarms) {
        QJsonObject obj;
        obj["alarmId"]         = e.alarmId;
        obj["dateTime"]        = e.dateTime.toString(Qt::ISODate);
        obj["enabled"]         = e.enabled;
        obj["soundFile"]       = e.soundFile;
        obj["dismissMode"]     = e.dismissMode;
        obj["gameType"]        = e.gameType;
        obj["repeatMask"]      = e.repeatMask;
        obj["useSpecificDate"] = e.useSpecificDate;
        obj["logFile"]         = e.logFile;
        arr.append(obj);
    }
    QJsonDocument doc(arr);
    QFile f(ALARMS_SAVE_FILE);
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        f.write(doc.toJson(QJsonDocument::Compact));
        f.close();
    }
}

void MainWindow::loadAlarms()
{
    QFile f(ALARMS_SAVE_FILE);
    if (!f.open(QIODevice::ReadOnly)) return;
    const QByteArray data = f.readAll();
    f.close();

    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(data, &err);
    if (err.error != QJsonParseError::NoError || !doc.isArray()) return;

    const QDateTime now = QDateTime::currentDateTime();
    const QJsonArray arr = doc.array();
    for (const QJsonValue &val : arr) {
        if (!val.isObject()) continue;
        const QJsonObject obj = val.toObject();

        AlarmEntry e;
        e.alarmId         = obj["alarmId"].toInt();
        e.dateTime        = QDateTime::fromString(obj["dateTime"].toString(), Qt::ISODate);
        e.enabled         = obj["enabled"].toBool(true);
        e.soundFile       = obj["soundFile"].toString(QDir::homePath() + "/test_contents/test.wav");
        e.dismissMode     = obj["dismissMode"].toInt(0);
        e.gameType        = obj["gameType"].toInt(0);
        e.repeatMask      = obj["repeatMask"].toInt(0);
        e.useSpecificDate = obj["useSpecificDate"].toBool(false);
        e.logFile         = obj["logFile"].toString(alarmLogPath(e.alarmId));

        if (!e.dateTime.isValid()) continue;

        // Non-repeat alarms that have already passed: push to tomorrow
        if (e.repeatMask == 0 && e.enabled && e.dateTime <= now) {
            e.enabled = false;   // expired one-shot alarm — keep it but disabled
        }
        // Repeat alarms: advance to next valid occurrence
        if (e.repeatMask != 0 && e.enabled && e.dateTime <= now) {
            e.dateTime = computeNextRepeatDateTime(now, e.dateTime.time(), e.repeatMask);
        }

        m_alarms.append(e);
        // Keep m_nextAlarmId ahead of all loaded IDs
        if (e.alarmId >= m_nextAlarmId)
            m_nextAlarmId = e.alarmId + 1;
    }

    sortAlarmsByTime();
    refreshAlarmList();
}
