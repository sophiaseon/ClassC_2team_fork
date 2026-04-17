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
#include <QtConcurrent/QtConcurrentRun>

// Buzzer ioctl constants (matches my_ioctl.h)
#define MY_IOCTL_MAGIC    'M'
#define IOCTL_PLAY_TETRIS _IO(MY_IOCTL_MAGIC, 0)
#define IOCTL_STOP        _IO(MY_IOCTL_MAGIC, 3)
#define BUZZER_DEVICE     "/dev/mybuzzer"
#include <QComboBox>
#include <QDir>
#include <QPointer>
#include <QDialog>
#include <QFile>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QProcess>
#include <QPushButton>
#include <QTextStream>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

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
    , m_exitButton(nullptr)
    , m_alarmPlayer(new QProcess(this))
    , m_buttonWatcher(new ButtonWatcher(this))
{
    // Open buzzer device lazily on first use (insmod may not have happened yet)
    // Install SIGUSR1 handler: empty (prevents process termination) but wakes
    // msleep_interruptible in the kernel so the buzzer thread exits quickly.
    ::signal(SIGUSR1, [](int){});
    buildUi();

    m_actionTimer.start();

    connect(m_clockTimer,         &QTimer::timeout,     this, &MainWindow::updateCurrentTime);
    connect(m_exitButton,         &QPushButton::clicked, this, &MainWindow::close);
    connect(m_addButton,          &QPushButton::clicked, this, &MainWindow::openAddDialog);
    connect(m_editButton,         &QPushButton::clicked, this, &MainWindow::openEditDialog);
    connect(m_deleteButton,       &QPushButton::clicked, this, &MainWindow::deleteSelectedAlarm);
    connect(m_debugPlus5SecButton,&QPushButton::clicked, this, &MainWindow::setDebugAlarmPlus5Sec);
    connect(m_statButton,         &QPushButton::clicked, this, &MainWindow::openStatDialog);

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
    setWindowTitle("Alarm Clock");
    setMinimumSize(960, 640);
    resize(1024, 680);

    m_centralWidget = new QWidget(this);
    m_centralWidget->setStyleSheet(
        "QWidget { background: #f5f7fb; color: #111827; }"
        "QLabel#HeaderTitle { font-size: 30px; font-weight: 800; color: #0f172a; }"
        "QLabel#HeaderSubTitle { font-size: 14px; color: #6b7280; }"
        "QLabel#TimeBadge {"
        "    font-size: 26px; font-weight: 800; color: #0f172a;"
        "    background: #ffffff; border: 1px solid #e5e7eb;"
        "    border-radius: 18px; padding: 10px 18px;"
        "}"
        "QLabel#CountLabel { font-size: 14px; font-weight: 600; color: #64748b; }"
        "QPushButton#PrimaryButton {"
        "    font-size: 16px; font-weight: 700; color: #ffffff;"
        "    background: #3182f6; border: none; border-radius: 14px; padding: 0 20px;"
        "}"
        "QPushButton#PrimaryButton:pressed { background: #2272e6; }"
        "QPushButton#SecondaryButton {"
        "    font-size: 15px; font-weight: 700; color: #334155;"
        "    background: #e2e8f0; border: none; border-radius: 14px; padding: 0 18px;"
        "}"
        "QPushButton#SecondaryButton:pressed { background: #cbd5e1; }"
        "QPushButton#UtilityButton {"
        "    font-size: 13px; font-weight: 700; color: #475569;"
        "    background: #eef2ff; border: 1px solid #dbeafe; border-radius: 11px; padding: 0 14px;"
        "}"
        "QPushButton#UtilityButton:pressed { background: #dbeafe; }"
        "QPushButton#DangerUtilityButton {"
        "    font-size: 13px; font-weight: 700; color: #b91c1c;"
        "    background: #fef2f2; border: 1px solid #fecaca; border-radius: 11px; padding: 0 14px;"
        "}"
        "QPushButton#DangerUtilityButton:pressed { background: #fee2e2; }"
    );
    setCentralWidget(m_centralWidget);

    QVBoxLayout *root = new QVBoxLayout(m_centralWidget);
    root->setContentsMargins(28, 22, 28, 22);
    root->setSpacing(14);

    QHBoxLayout *topBar = new QHBoxLayout();
    topBar->setSpacing(12);

    QWidget *titleWrap = new QWidget(this);
    QVBoxLayout *titleLayout = new QVBoxLayout(titleWrap);
    titleLayout->setContentsMargins(0, 0, 0, 0);
    titleLayout->setSpacing(2);

    QLabel *titleLabel = new QLabel("MOMS", this);
    titleLabel->setObjectName("HeaderTitle");
    QLabel *subTitleLabel = new QLabel("Make Our Morning Successful", this);
    subTitleLabel->setObjectName("HeaderSubTitle");
    titleLayout->addWidget(titleLabel);
    titleLayout->addWidget(subTitleLabel);

    m_currentTimeLabel = new QLabel(this);
    m_currentTimeLabel->setAlignment(Qt::AlignCenter);
    m_currentTimeLabel->setObjectName("TimeBadge");

    m_debugPlus5SecButton = new QPushButton("+5s", this);
    m_debugPlus5SecButton->setObjectName("UtilityButton");
    m_debugPlus5SecButton->setFixedHeight(40);

    m_statButton = new QPushButton("Stats", this);
    m_statButton->setObjectName("UtilityButton");
    m_statButton->setFixedHeight(40);

    m_exitButton = new QPushButton("Exit", this);
    m_exitButton->setObjectName("DangerUtilityButton");
    m_exitButton->setFixedHeight(40);

    topBar->addWidget(titleWrap, 1);
    topBar->addStretch();
    topBar->addWidget(m_currentTimeLabel);
    topBar->addSpacing(10);
    topBar->addWidget(m_debugPlus5SecButton);
    topBar->addWidget(m_statButton);
    topBar->addWidget(m_exitButton);

    m_alarmCountLabel = new QLabel("No alarms yet", this);
    m_alarmCountLabel->setObjectName("CountLabel");
    m_alarmCountLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    m_alarmListWidget = new QListWidget(this);
    m_alarmListWidget->setStyleSheet(
        "QListWidget {"
        "    background: transparent; border: none;"
        "    color: #0f172a; font-size: 16px; padding: 4px; outline: none;"
        "}"
        "QListWidget::item { margin: 6px 0; }"
        "QListWidget::item:selected { background: #dbeafe; border-radius: 18px; }"
    );
    m_alarmListWidget->setSpacing(2);

    QHBoxLayout *btnRow = new QHBoxLayout();
    btnRow->setSpacing(12);

    auto makeBtn = [this](const QString &label, const QString &objectName) -> QPushButton * {
        QPushButton *btn = new QPushButton(label, this);
        btn->setObjectName(objectName);
        btn->setFixedHeight(56);
        return btn;
    };

    m_addButton    = makeBtn("+ Add Alarm", "PrimaryButton");
    m_editButton   = makeBtn("Edit", "SecondaryButton");
    m_deleteButton = makeBtn("Delete", "SecondaryButton");

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
                  if (a.enabled != b.enabled) return a.enabled > b.enabled;
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
        const QString timeText = e.dateTime.toString("hh:mm");
        QString mainText = QString("%1  ·  %2").arg(timeText, e.dateTime.toString("yyyy-MM-dd"));
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
        rowWidget->setStyleSheet(
            QString(
                "QWidget {"
                "    background: %1;"
                "    border: 1px solid %2;"
                "    border-radius: 16px;"
                "}"
            ).arg(e.enabled ? "#ffffff" : "#f8fafc",
                  e.enabled ? "#e2e8f0" : "#e5e7eb")
        );
        QHBoxLayout *rowLayout = new QHBoxLayout(rowWidget);
        rowLayout->setContentsMargins(16, 10, 16, 10);
        rowLayout->setSpacing(10);

        QWidget *textWrap = new QWidget(rowWidget);
        QVBoxLayout *textLayout = new QVBoxLayout(textWrap);
        textLayout->setContentsMargins(0, 0, 0, 0);
        textLayout->setSpacing(2);

        QLabel *mainLbl = new QLabel(mainText, textWrap);
        mainLbl->setStyleSheet(
            QString("QLabel { font-size: 20px; font-weight: 800; color: %1; background: transparent; border: none; }")
                .arg(e.enabled ? "#0f172a" : "#94a3b8")
        );
        QLabel *subLbl = new QLabel(subText, textWrap);
        subLbl->setStyleSheet(
            QString("QLabel { font-size: 13px; font-weight: 600; color: %1; background: transparent; border: none; }")
                .arg(e.enabled ? "#3182f6" : "#9ca3af")
        );

        textLayout->addWidget(mainLbl);
        textLayout->addWidget(subLbl);

        QPushButton *enableBtn = new QPushButton(rowWidget);
        enableBtn->setFixedSize(90, 36);
        if (e.enabled) {
            enableBtn->setText("Off");
            enableBtn->setStyleSheet(
                "QPushButton { font-size: 13px; font-weight: 700; color: #b91c1c;"
                "    background: #fee2e2; border: 1px solid #fecaca; border-radius: 10px; }"
                "QPushButton:pressed { background: #fecaca; }"
            );
            connect(enableBtn, &QPushButton::clicked, this, [this, i]() {
                if (i < 0 || i >= m_alarms.size()) return;
                m_alarms[i].enabled = false;
                refreshAlarmList();
            });
        } else {
            enableBtn->setText("On");
            enableBtn->setStyleSheet(
                "QPushButton { font-size: 13px; font-weight: 700; color: white;"
                "    background: #3182f6; border: none; border-radius: 10px; }"
                "QPushButton:pressed { background: #2272e6; }"
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

                refreshAlarmList();
            });
        }

        rowLayout->addWidget(textWrap, 1);
        rowLayout->addWidget(enableBtn, 0, Qt::AlignVCenter);

        QPushButton *statBtn = new QPushButton("Log", rowWidget);
        statBtn->setFixedSize(64, 36);
        statBtn->setStyleSheet(
            "QPushButton { font-size: 13px; font-weight: 700; color: #334155;"
            "    background: #e2e8f0; border: none; border-radius: 10px; }"
            "QPushButton:pressed { background: #cbd5e1; }"
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
        item->setSizeHint(QSize(0, 92));
        m_alarmListWidget->addItem(item);
        m_alarmListWidget->setItemWidget(item, rowWidget);

        if (e.enabled) ++activeCount;
    }

    if (m_alarms.isEmpty()) {
        m_alarmCountLabel->setText("No alarms set");
    } else {
        m_alarmCountLabel->setText(
            QString("%1 alarm(s) · %2 active").arg(m_alarms.size()).arg(activeCount)
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

    // Play sound (first triggered alarm's choice)
    const QString &soundFile = triggered.first().soundFile;
    if (soundFile == "buzzer:tetris") {
        startBuzzerTetris();
    } else {
        if (m_alarmPlayer->state() == QProcess::NotRunning) {
            m_alarmPlayer->start("aplay", QStringList() << soundFile);
        }
    }

    // Collect alarm times and determine mode priority: Camera > Button > Game > Simple
    QStringList alarmTimes;
    bool needGame = false;
    bool needButton = false;
    bool needCamera = false;
    DismissDialog::GameType selectedGameType = DismissDialog::NumberOrder;
    for (const TriggeredInfo &t : triggered) {
        alarmTimes << t.alarmTime;
        if (t.dismissMode == AlarmDialog::DismissGame) {
            needGame = true;
            selectedGameType = (t.gameType == AlarmDialog::GameColorMemory)
                ? DismissDialog::ColorMemory
                : DismissDialog::NumberOrder;
        }
        if (t.dismissMode == AlarmDialog::DismissButton) needButton = true;
        if (t.dismissMode == AlarmDialog::DismissCamera) needCamera = true;
    }

    DismissDialog::Mode dlgMode = DismissDialog::Simple;
    if (needCamera) dlgMode = DismissDialog::Camera;
    else if (needButton) dlgMode = DismissDialog::Button;
    else if (needGame) dlgMode = DismissDialog::Game;

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

    const QString capturedPhotoPath = dlg.capturedPhotoPath();

    // Write dismiss record to log
    {
        const QString dismissTime =
            QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
        QDir().mkpath("/mnt/nfs/capture");
        QFile logFile("/mnt/nfs/alarm.txt");
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
            const QString alarmLogPath =
                QString("/mnt/nfs/capture/alarm_%1.txt").arg(t.alarmId);
            QFile alarmLog(alarmLogPath);
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
    m_alarms.append(entry);
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

// ── openEditDialog ────────────────────────────────────────────────────────────
void MainWindow::openEditDialog()
{
    m_editButton->setEnabled(false);
    QTimer::singleShot(500, this, [this]() { m_editButton->setEnabled(true); });

    const int row = m_alarmListWidget->currentRow();
    if (row < 0 || row >= m_alarms.size()) {
        QMessageBox::information(this, "Edit Alarm", "Please select an alarm to edit.");
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
    refreshAlarmList();
}

// ── setDebugAlarmPlus5Sec ─────────────────────────────────────────────────────
void MainWindow::setDebugAlarmPlus5Sec()
{
    m_debugPlus5SecButton->setEnabled(false);
    QTimer::singleShot(500, this, [this]() { m_debugPlus5SecButton->setEnabled(true); });

    QDialog dlg(this);
    dlg.setWindowTitle("Quick Alarm (+5s)");
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
    soundCombo->addItem("test.wav",  "/mnt/nfs/test_contents/test.wav");
    soundCombo->addItem("test2.wav", "/mnt/nfs/test_contents/test2.wav");
    soundCombo->setFixedHeight(40);
    root->addWidget(soundLabel);
    root->addWidget(soundCombo);

    QLabel *modeLabel = new QLabel("Dismiss Mode", &dlg);
    modeLabel->setStyleSheet("QLabel { font-size: 13px; color: #aaaaaa; }");
    QComboBox *modeCombo = new QComboBox(&dlg);
    modeCombo->addItem("Simple", AlarmDialog::DismissSimple);
    modeCombo->addItem("Game", AlarmDialog::DismissGame);
    modeCombo->addItem("Button", AlarmDialog::DismissButton);
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
    m_alarms.append(entry);
    sortAlarmsByTime();
    refreshAlarmList();
}

// ── openStatDialog ────────────────────────────────────────────────────────────
void MainWindow::openStatDialog()
{
    m_statButton->setEnabled(false);
    QTimer::singleShot(500, this, [this]() { m_statButton->setEnabled(true); });

    StatDialog dlg("/mnt/nfs/alarm.txt", this);
    dlg.exec();
}

// ── openAlarmStatDialog ───────────────────────────────────────────────────────
void MainWindow::openAlarmStatDialog(int alarmIndex)
{
    if (alarmIndex < 0 || alarmIndex >= m_alarms.size()) return;
    const AlarmEntry &e = m_alarms.at(alarmIndex);
    const QString logPath =
        QString("/mnt/nfs/capture/alarm_%1.txt").arg(e.alarmId);
    const QString repeatText = repeatMaskToText(e.repeatMask);
    const QString timeStr = e.dateTime.toString("hh:mm");
    const QString title = repeatText.isEmpty()
        ? QString("Alarm #%1 (%2) Statistics").arg(alarmIndex + 1).arg(timeStr)
        : QString("Alarm #%1 (%2  [%3]) Statistics")
              .arg(alarmIndex + 1).arg(timeStr).arg(repeatText);
    StatDialog dlg(logPath, this, title);
    dlg.exec();
}
