#include "dismissdialog.h"
#include "alarmcamerathread.h"
#include "ultrasonicwatcher.h"

#include <QCloseEvent>
#include <QDateTime>
#include <QDir>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPixmap>
#include <QPushButton>
#include <QRandomGenerator>
#include <QTimer>
#include <QVBoxLayout>

DismissDialog::DismissDialog(const QStringList &alarmTimes,
                             Mode mode,
                             GameType gameType,
                             QWidget *parent,
                             int alarmId)
    : QDialog(parent)
    , m_mode(mode)
    , m_gameType(gameType)
    , m_nextExpected(1)
    , m_progressLabel(nullptr)
    , m_colorStatusLabel(nullptr)
    , m_colorPreviewLabel(nullptr)
    , m_colorRound(0)
    , m_colorShowIndex(0)
    , m_colorInputIndex(0)
    , m_showingSequence(false)
    , m_cameraThread(nullptr)
    , m_cameraStatusLabel(nullptr)
    , m_cameraPreviewLabel(nullptr)
    , m_captureRequested(false)
    , m_alarmId(alarmId)
    , m_gameEngine(nullptr)
    , m_btnCountLabel(nullptr)
    , m_btnTargetLabel(nullptr)
    , m_btnCountdownLabel(nullptr)
    , m_btnStatusLabel(nullptr)
    , m_usWatcher(nullptr)
    , m_usStep(0)
    , m_usNeedRelease(false)
    , m_usHolding(false)
    , m_usTargetLabel(nullptr)
    , m_usProgressLabel(nullptr)
    , m_usStatusLabel(nullptr)
{
    for (int i = 0; i < 25; ++i) m_numButtons[i] = nullptr;
    for (int i = 0; i < 4; ++i)  m_colorButtons[i]  = nullptr;
    for (int i = 0; i < 4; ++i)  m_usSeq[i]         = i;
    for (int i = 0; i < 4; ++i)  m_usStepLabels[i]  = nullptr;
    for (int i = 0; i < 4; ++i)  m_usDistLabels[i]  = nullptr;

    m_actionTimer.start();

    setWindowTitle("Alarm!");
    setStyleSheet("QDialog { background: #0b0b0b; border: 2px solid #ffffff; border-radius: 4px; }");

    // Block OS close button for game/button/camera mode
    if (m_mode == Game || m_mode == Button || m_mode == Camera) {
        setWindowFlags(windowFlags() & ~Qt::WindowCloseButtonHint);
    }

    if (m_mode == Simple) {
        buildSimpleUi(alarmTimes);
    } else if (m_mode == Game) {
        buildGameUi(alarmTimes);
    } else if (m_mode == Camera) {
        buildCameraUi(alarmTimes);
    } else {
        buildButtonUi(alarmTimes);
    }
}

DismissDialog::~DismissDialog()
{
    if (m_usWatcher) {
        delete m_usWatcher;
        m_usWatcher = nullptr;
    }
    if (m_cameraThread) {
        m_cameraThread->stop();
        delete m_cameraThread;
        m_cameraThread = nullptr;
    }
}

QString DismissDialog::capturedPhotoPath() const
{
    return m_capturedPhotoPath;
}

void DismissDialog::closeEvent(QCloseEvent *event)
{
    if (m_mode == Game || m_mode == Button || m_mode == Camera) {
        event->ignore();
    } else {
        event->accept();
    }
}

void DismissDialog::reject()
{
    if (m_mode == Game || m_mode == Button || m_mode == Camera) {
        return;
    }
    QDialog::reject();
}

// ── Simple UI ─────────────────────────────────────────────────────────────────
void DismissDialog::buildSimpleUi(const QStringList &alarmTimes)
{
    setFixedSize(460, 220);

    QVBoxLayout *root = new QVBoxLayout(this);
    root->setContentsMargins(24, 20, 24, 20);
    root->setSpacing(14);

    QLabel *titleLabel = new QLabel("Alarm is ringing!", this);
    titleLabel->setAlignment(Qt::AlignCenter);
    titleLabel->setStyleSheet(
        "QLabel { font-size: 20px; font-weight: 700; color: #ff6666; }"
    );
    root->addWidget(titleLabel);

    QLabel *timesLabel = new QLabel(alarmTimes.join("\n"), this);
    timesLabel->setAlignment(Qt::AlignCenter);
    timesLabel->setStyleSheet(
        "QLabel { font-size: 15px; color: #dddddd; }"
    );
    root->addWidget(timesLabel);

    root->addStretch();

    QPushButton *dismissBtn = new QPushButton("Dismiss", this);
    dismissBtn->setFixedHeight(52);
    dismissBtn->setStyleSheet(
        "QPushButton { font-size: 18px; font-weight: 700; color: white;"
        "    background: #2d7dff; border: none; border-radius: 10px; }"
        "QPushButton:pressed { background: #1d5fc7; }"
    );
    connect(dismissBtn, &QPushButton::clicked, this, [this]() {
        if (m_actionTimer.elapsed() < 300) return;
        m_actionTimer.restart();
        accept();
    });
    root->addWidget(dismissBtn);
}

void DismissDialog::buildGameUi(const QStringList &alarmTimes)
{
    if (m_gameType == Ultrasonic) {
        buildUltrasonicUi(alarmTimes);
    } else if (m_gameType == ColorMemory) {
        buildColorMemoryGameUi(alarmTimes);
    } else {
        buildNumberOrderGameUi(alarmTimes);
    }
}

// ── Number Order Game UI ──────────────────────────────────────────────────────
void DismissDialog::buildNumberOrderGameUi(const QStringList &alarmTimes)
{
    setFixedSize(500, 580);

    QVBoxLayout *root = new QVBoxLayout(this);
    root->setContentsMargins(20, 14, 20, 14);
    root->setSpacing(8);

    QLabel *titleLabel = new QLabel("🔔  Alarm is ringing!", this);
    titleLabel->setAlignment(Qt::AlignCenter);
    titleLabel->setStyleSheet(
        "QLabel { font-size: 18px; font-weight: 700; color: #ff6666; }"
    );
    root->addWidget(titleLabel);

    QLabel *timesLabel = new QLabel(alarmTimes.join("  /  "), this);
    timesLabel->setAlignment(Qt::AlignCenter);
    timesLabel->setStyleSheet(
        "QLabel { font-size: 13px; color: #aaaaaa; }"
    );
    root->addWidget(timesLabel);

    QLabel *instrLabel = new QLabel("Game: press numbers 1 to 25 in order", this);
    instrLabel->setAlignment(Qt::AlignCenter);
    instrLabel->setStyleSheet(
        "QLabel { font-size: 12px; color: #888888; }"
    );
    root->addWidget(instrLabel);

    m_progressLabel = new QLabel("Next: 1", this);
    m_progressLabel->setAlignment(Qt::AlignCenter);
    m_progressLabel->setStyleSheet(
        "QLabel { font-size: 16px; font-weight: 700; color: #2d7dff; }"
    );
    root->addWidget(m_progressLabel);

    // Shuffle numbers 1-25
    QList<int> nums;
    for (int i = 1; i <= 25; ++i) nums.append(i);
    for (int i = nums.size() - 1; i > 0; --i) {
        int j = static_cast<int>(QRandomGenerator::global()->bounded(static_cast<quint32>(i + 1)));
        nums.swapItemsAt(i, j);
    }

    // Build 5x5 grid
    QGridLayout *grid = new QGridLayout();
    grid->setSpacing(7);
    grid->setContentsMargins(0, 0, 0, 10); // Adjust bottom margin to prevent button cutoff

    for (int pos = 0; pos < 25; ++pos) {
        const int num = nums.at(pos);

        QPushButton *btn = new QPushButton(QString::number(num), this);
        btn->setFixedSize(64, 64);
        btn->setStyleSheet(
            "QPushButton { font-size: 18px; font-weight: 700; color: white;"
            "    background: #2c2c2c; border: 1px solid #4a4a4a; border-radius: 10px; }"
            "QPushButton:pressed { background: #4a4a4a; }"
        );

        m_numButtons[num - 1] = btn; // index by value

        connect(btn, &QPushButton::clicked, this, [this, num]() {
            onNumberClicked(num);
        });

        grid->addWidget(btn, pos / 5, pos % 5);
    }

    root->addLayout(grid);
}

// ── Number Order game logic ────────────────────────────────────────────────────
void DismissDialog::onNumberClicked(int number)
{
    if (m_actionTimer.elapsed() < 300) return;
    m_actionTimer.restart();

    QPushButton *btn = m_numButtons[number - 1];

    if (number == m_nextExpected) {
        // Correct
        btn->setEnabled(false);
        btn->setStyleSheet(
            "QPushButton { font-size: 18px; font-weight: 700; color: #2a2a2a;"
            "    background: #1a4a1a; border: 1px solid #2d6a2d; border-radius: 10px; }"
        );
        ++m_nextExpected;

        if (m_nextExpected > 25) {
            accept();
        } else {
            m_progressLabel->setText(QString("Next: %1").arg(m_nextExpected));
        }
    } else {
        // Wrong — flash red then restore
        btn->setStyleSheet(
            "QPushButton { font-size: 18px; font-weight: 700; color: white;"
            "    background: #993333; border: 1px solid #cc3333; border-radius: 10px; }"
        );
        QTimer::singleShot(350, this, [this, number]() {
            if (m_numButtons[number - 1] && m_numButtons[number - 1]->isEnabled()) {
                m_numButtons[number - 1]->setStyleSheet(
                    "QPushButton { font-size: 18px; font-weight: 700; color: white;"
                    "    background: #2c2c2c; border: 1px solid #4a4a4a; border-radius: 10px; }"
                    "QPushButton:pressed { background: #4a4a4a; }"
                );
            }
        });
    }
}

// ── Color Memory game UI ──────────────────────────────────────────────────────
void DismissDialog::buildColorMemoryGameUi(const QStringList &alarmTimes)
{
    setFixedSize(760, 430);

    QVBoxLayout *root = new QVBoxLayout(this);
    root->setContentsMargins(20, 14, 20, 14);
    root->setSpacing(8);

    QLabel *titleLabel = new QLabel("Alarm is ringing!", this);
    titleLabel->setAlignment(Qt::AlignCenter);
    titleLabel->setStyleSheet(
        "QLabel { font-size: 20px; font-weight: 700; color: #ff6666; }"
    );
    root->addWidget(titleLabel);

    QLabel *timesLabel = new QLabel(alarmTimes.join("  /  "), this);
    timesLabel->setAlignment(Qt::AlignCenter);
    timesLabel->setStyleSheet(
        "QLabel { font-size: 13px; color: #aaaaaa; }"
    );
    root->addWidget(timesLabel);

    m_colorStatusLabel = new QLabel(this);
    m_colorStatusLabel->setAlignment(Qt::AlignCenter);
    m_colorStatusLabel->setStyleSheet(
        "QLabel { font-size: 15px; font-weight: 700; color: #cfd8dc; }"
    );
    root->addWidget(m_colorStatusLabel);

    QWidget *body = new QWidget(this);
    QHBoxLayout *bodyLayout = new QHBoxLayout(body);
    bodyLayout->setContentsMargins(0, 4, 0, 0);
    bodyLayout->setSpacing(16);

    QWidget *leftPanel = new QWidget(body);
    QVBoxLayout *leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->setSpacing(8);

    QLabel *previewTitle = new QLabel("Sequence Output", leftPanel);
    previewTitle->setAlignment(Qt::AlignCenter);
    previewTitle->setStyleSheet("QLabel { font-size: 14px; color: #9e9e9e; }");

    m_colorPreviewLabel = new QLabel("READY", leftPanel);
    m_colorPreviewLabel->setAlignment(Qt::AlignCenter);
    m_colorPreviewLabel->setMinimumSize(240, 240);
    m_colorPreviewLabel->setStyleSheet(
        "QLabel {"
        "  font-size: 30px; font-weight: 800; color: #101010;"
        "  background: #2d2d2d; border: 2px solid #4a4a4a; border-radius: 14px;"
        "}"
    );

    leftLayout->addWidget(previewTitle);
    leftLayout->addWidget(m_colorPreviewLabel, 1);

    QWidget *rightPanel = new QWidget(body);
    QGridLayout *grid = new QGridLayout(rightPanel);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setSpacing(10);

    const QString labels[4] = { "RED", "BLUE", "YELLOW", "GREEN" };
    const QString bases[4]  = {
        "#d94343", "#3f6ed6", "#d9bf3f", "#46a446"
    };

    for (int i = 0; i < 4; ++i) {
        QPushButton *btn = new QPushButton(labels[i], rightPanel);
        btn->setMinimumSize(180, 110);
        btn->setStyleSheet(QString(
            "QPushButton { font-size: 22px; font-weight: 800; color: #111111;"
            "  background: %1; border: 2px solid #2a2a2a; border-radius: 12px; }"
            "QPushButton:pressed { border: 3px solid white; }"
        ).arg(bases[i]));
        connect(btn, &QPushButton::clicked, this, [this, i]() { onColorClicked(i); });
        m_colorButtons[i] = btn;
        grid->addWidget(btn, i / 2, i % 2);
    }

    bodyLayout->addWidget(leftPanel, 1);
    bodyLayout->addWidget(rightPanel, 1);
    root->addWidget(body, 1);

    startColorMemoryRound();
}

void DismissDialog::startColorMemoryRound()
{
    static const int roundLengths[3] = { 4, 4, 4 };

    if (m_colorRound >= 3) {
        accept();
        return;
    }

    m_colorSequence.clear();
    const int len = roundLengths[m_colorRound];
    for (int i = 0; i < len; ++i) {
        m_colorSequence.append(static_cast<int>(QRandomGenerator::global()->bounded(4u)));
    }

    m_colorShowIndex = 0;
    m_colorInputIndex = 0;
    m_showingSequence = true;

    for (int i = 0; i < 4; ++i) {
        if (m_colorButtons[i]) m_colorButtons[i]->setEnabled(false);
    }

    m_colorStatusLabel->setText(
        QString("Round %1/3 - Watch %2 colors carefully").arg(m_colorRound + 1).arg(len)
    );
    m_colorPreviewLabel->setText("WATCH");
    setPreviewColor(-1, false);

    QTimer::singleShot(350, this, &DismissDialog::showColorMemoryStep);
}

void DismissDialog::showColorMemoryStep()
{
    if (m_colorShowIndex >= m_colorSequence.size()) {
        m_showingSequence = false;
        m_colorInputIndex = 0;
        m_colorStatusLabel->setText("Now repeat the sequence");
        setPreviewColor(-1, false);
        for (int i = 0; i < 4; ++i) {
            if (m_colorButtons[i]) m_colorButtons[i]->setEnabled(true);
        }
        return;
    }

    const int color = m_colorSequence[m_colorShowIndex];
    setPreviewColor(color, true);

    QTimer::singleShot(700, this, [this]() {  // color display time: 0.7s
        setPreviewColor(-1, false);
        ++m_colorShowIndex;
        QTimer::singleShot(160, this, &DismissDialog::showColorMemoryStep);
    });
}

void DismissDialog::setPreviewColor(int colorIndex, bool active)
{
    if (!active || colorIndex < 0 || colorIndex > 3) {
        m_colorPreviewLabel->setStyleSheet(
            "QLabel {"
            "  font-size: 30px; font-weight: 800; color: #bdbdbd;"
            "  background: #2d2d2d; border: 2px solid #4a4a4a; border-radius: 14px;"
            "}"
        );
        return;
    }

    const char *labels[4] = { "RED", "BLUE", "YELLOW", "GREEN" };
    const char *colors[4] = { "#d94343", "#3f6ed6", "#d9bf3f", "#46a446" };
    m_colorPreviewLabel->setText(labels[colorIndex]);
    m_colorPreviewLabel->setStyleSheet(QString(
        "QLabel {"
        "  font-size: 30px; font-weight: 800; color: #111111;"
        "  background: %1; border: 2px solid #f0f0f0; border-radius: 14px;"
        "}"
    ).arg(colors[colorIndex]));
}

void DismissDialog::onColorClicked(int colorIndex)
{
    if (m_actionTimer.elapsed() < 300) return;
    m_actionTimer.restart();

    if (m_showingSequence) return;
    if (m_colorInputIndex >= m_colorSequence.size()) return;

    if (colorIndex == m_colorSequence[m_colorInputIndex]) {
        ++m_colorInputIndex;
        m_colorStatusLabel->setText(
            QString("Correct %1/%2").arg(m_colorInputIndex).arg(m_colorSequence.size())
        );

        if (m_colorInputIndex == m_colorSequence.size()) {
            ++m_colorRound;
            if (m_colorRound >= 3) {
                accept();
                return;
            }
            m_colorStatusLabel->setText(
                QString("Round cleared. Next round: %1/3").arg(m_colorRound + 1)
            );
            for (int i = 0; i < 4; ++i) {
                if (m_colorButtons[i]) m_colorButtons[i]->setEnabled(false);
            }
            QTimer::singleShot(700, this, &DismissDialog::startColorMemoryRound);
        }
    } else {
        m_colorStatusLabel->setText(
            QString("Wrong sequence. Retry round %1").arg(m_colorRound + 1)
        );
        for (int i = 0; i < 4; ++i) {
            if (m_colorButtons[i]) m_colorButtons[i]->setEnabled(false);
        }
        QTimer::singleShot(700, this, &DismissDialog::startColorMemoryRound);
    }
}

// ── dismiss() — no longer used for Button mode (kept for API compat) ─────────
void DismissDialog::dismiss()
{
    // Button mode now uses the GameEngine. dismiss() is a no-op.
}

void DismissDialog::onButtonPressedForGame()
{
    if (m_mode != Button || !m_gameEngine) return;
    m_gameEngine->onButtonPressed();
}

void DismissDialog::captureByButton()
{
    if (m_mode != Camera) return;

    if (m_captureRequested) return;
    m_captureRequested = true;

    if (!m_cameraThread) {
        m_cameraStatusLabel->setText("Camera not available. Dismissing without photo.");
        accept();
        return;
    }

    const QString ts = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
    const QString fileName = m_alarmId >= 0
        ? QString("alarm_%1_%2.jpg").arg(m_alarmId).arg(ts)
        : QString("%1.jpg").arg(ts);
    const QString photoPath = QDir::homePath() + QString("/capture/%1").arg(fileName);

    m_cameraStatusLabel->setText("Capturing... keep still");
    m_cameraThread->requestCapture(photoPath);
}

// ── Button mode UI (GameEngine-based) ───────────────────────────────────────
void DismissDialog::buildButtonUi(const QStringList &alarmTimes)
{
    setFixedSize(480, 440);

    QVBoxLayout *root = new QVBoxLayout(this);
    root->setContentsMargins(24, 16, 24, 16);
    root->setSpacing(8);

    QLabel *titleLabel = new QLabel("Alarm is ringing!", this);
    titleLabel->setAlignment(Qt::AlignCenter);
    titleLabel->setStyleSheet(
        "QLabel { font-size: 20px; font-weight: 700; color: #ff6666; }"
    );
    root->addWidget(titleLabel);

    QLabel *timesLabel = new QLabel(alarmTimes.join("  /  "), this);
    timesLabel->setAlignment(Qt::AlignCenter);
    timesLabel->setStyleSheet("QLabel { font-size: 13px; color: #aaaaaa; }");
    root->addWidget(timesLabel);

    // Countdown
    m_btnCountdownLabel = new QLabel(QString::number(GameEngine::GAME_DURATION), this);
    m_btnCountdownLabel->setAlignment(Qt::AlignCenter);
    m_btnCountdownLabel->setStyleSheet(
        "QLabel { font-size: 56px; font-weight: 700; color: #ffffff; }"
    );
    root->addWidget(m_btnCountdownLabel);

    // Current press count (big)
    m_btnCountLabel = new QLabel("0", this);
    m_btnCountLabel->setAlignment(Qt::AlignCenter);
    m_btnCountLabel->setStyleSheet(
        "QLabel { font-size: 80px; font-weight: 800; color: #2d7dff; }"
    );
    root->addWidget(m_btnCountLabel);

    // Progress
    m_btnTargetLabel = new QLabel("0 / ?", this);
    m_btnTargetLabel->setAlignment(Qt::AlignCenter);
    m_btnTargetLabel->setStyleSheet(
        "QLabel { font-size: 20px; font-weight: 600; color: #aaaaaa; }"
    );
    root->addWidget(m_btnTargetLabel);

    // Status message
    m_btnStatusLabel = new QLabel("Press the physical button!", this);
    m_btnStatusLabel->setAlignment(Qt::AlignCenter);
    m_btnStatusLabel->setWordWrap(true);
    m_btnStatusLabel->setStyleSheet(
        "QLabel { font-size: 15px; font-weight: 600; color: #2d7dff; }"
    );
    root->addWidget(m_btnStatusLabel);

    // Create and wire GameEngine
    m_gameEngine = new GameEngine(this);

    connect(m_gameEngine, &GameEngine::countUpdated,
            this, &DismissDialog::onButtonGameCountUpdated);
    connect(m_gameEngine, &GameEngine::countdownUpdated,
            this, &DismissDialog::onButtonGameCountdownUpdated);
    connect(m_gameEngine, &GameEngine::gameSuccess,
            this, &DismissDialog::onButtonGameSuccess);
    connect(m_gameEngine, &GameEngine::gameFailure,
            this, &DismissDialog::onButtonGameFailure);

    // Auto-start the game
    m_gameEngine->startGame();
}

void DismissDialog::buildCameraUi(const QStringList &alarmTimes)
{
    setFixedSize(760, 560);

    QVBoxLayout *root = new QVBoxLayout(this);
    root->setContentsMargins(20, 14, 20, 14);
    root->setSpacing(10);

    QLabel *titleLabel = new QLabel("Alarm is ringing!", this);
    titleLabel->setAlignment(Qt::AlignCenter);
    titleLabel->setStyleSheet(
        "QLabel { font-size: 20px; font-weight: 700; color: #ff6666; }"
    );
    root->addWidget(titleLabel);

    QLabel *timesLabel = new QLabel(alarmTimes.join("\n"), this);
    timesLabel->setAlignment(Qt::AlignCenter);
    timesLabel->setStyleSheet(
        "QLabel { font-size: 14px; color: #dddddd; }"
    );
    root->addWidget(timesLabel);

    m_cameraPreviewLabel = new QLabel(this);
    m_cameraPreviewLabel->setMinimumSize(640, 480);
    m_cameraPreviewLabel->setAlignment(Qt::AlignCenter);
    m_cameraPreviewLabel->setStyleSheet(
        "QLabel { background: #141414; border: 1px solid #3a3a3a; border-radius: 10px; color: #888888; }"
    );
    m_cameraPreviewLabel->setText("Starting camera...");
    root->addWidget(m_cameraPreviewLabel, 1);

    m_cameraStatusLabel = new QLabel("Press physical button to capture photo and dismiss", this);
    m_cameraStatusLabel->setAlignment(Qt::AlignCenter);
    m_cameraStatusLabel->setStyleSheet(
        "QLabel { font-size: 15px; font-weight: 700; color: #2d7dff; }"
    );
    root->addWidget(m_cameraStatusLabel);

    m_cameraThread = new AlarmCameraThread(this);
    connect(m_cameraThread, &AlarmCameraThread::frameReady, this, [this](const QImage &frame) {
        if (!m_cameraPreviewLabel) return;
        QPixmap px = QPixmap::fromImage(frame).scaled(
            m_cameraPreviewLabel->size(), Qt::KeepAspectRatio, Qt::FastTransformation);
        m_cameraPreviewLabel->setPixmap(px);
        // storeRelease: guarantees ARM memory ordering (plain assignment may reorder on ARM)
        m_cameraThread->m_uiReady.storeRelease(1);
    });
    connect(m_cameraThread, &AlarmCameraThread::captureSaved, this,
            [this](const QString &path, bool ok, const QString &errorText) {
                if (ok) {
                    m_capturedPhotoPath = path;
                    m_cameraStatusLabel->setText("Capture saved. Alarm dismissed.");
                    accept();
                } else {
                    m_captureRequested = false;
                    m_cameraStatusLabel->setText(QString("Capture failed: %1").arg(errorText));
                }
            });
    connect(m_cameraThread, &AlarmCameraThread::cameraError, this, [this](const QString &msg) {
        if (m_cameraPreviewLabel) m_cameraPreviewLabel->setText("Camera unavailable");
        if (m_cameraStatusLabel) m_cameraStatusLabel->setText(msg + " (button will dismiss without photo)");
    });
    connect(m_cameraThread, &AlarmCameraThread::captureRejectedLowLight, this,
            [this](float lux, float threshold) {
                m_captureRequested = false;
                if (m_cameraStatusLabel)
                    m_cameraStatusLabel->setText(
                        QString("Too dark to capture (%.1f lux, need %.0f). Press button again when brighter.")
                            .arg(static_cast<double>(lux))
                            .arg(static_cast<double>(threshold)));
            });
    connect(m_cameraThread, &AlarmCameraThread::statusUpdate, this,
            [this](const QString &msg) {
                if (m_cameraStatusLabel) m_cameraStatusLabel->setText(msg);
            });
    m_cameraThread->start();
}

// ── Button game slots ─────────────────────────────────────────────────────────
void DismissDialog::onButtonGameCountUpdated(int count)
{
    if (m_btnCountLabel)
        m_btnCountLabel->setText(QString::number(count));
    if (m_btnTargetLabel && m_gameEngine)
        m_btnTargetLabel->setText(
            QString("%1 / %2").arg(count).arg(m_gameEngine->targetCount()));
    // update status label immediately on each button press
    if (m_btnStatusLabel && m_gameEngine) {
        const int need = m_gameEngine->targetCount() - count;
        if (need > 0) {
            m_btnStatusLabel->setText(QString("Need %1 more press(es)!").arg(need));
            m_btnStatusLabel->setStyleSheet(
                "QLabel { font-size: 15px; font-weight: 600; color: #2d7dff; }");
        } else if (need == 0) {
            m_btnStatusLabel->setText("Hold on...  Dismissing soon!");
            m_btnStatusLabel->setStyleSheet(
                "QLabel { font-size: 15px; font-weight: 700; color: #66cc66; }");
        }
    }
}

void DismissDialog::onButtonGameCountdownUpdated(int secondsLeft)
{
    if (m_btnCountdownLabel)
        m_btnCountdownLabel->setText(QString::number(secondsLeft));
}

void DismissDialog::onButtonGameSuccess()
{
    if (m_btnStatusLabel) {
        m_btnStatusLabel->setText("SUCCESS!  Alarm dismissed.");
        m_btnStatusLabel->setStyleSheet(
            "QLabel { font-size: 18px; font-weight: 700; color: #66cc66; }");
    }
    if (m_btnCountdownLabel)
        m_btnCountdownLabel->setText("✓");
    QTimer::singleShot(800, this, [this]() { accept(); });
}

void DismissDialog::onButtonGameFailure()
{
    if (!m_gameEngine) return;

    const int count  = m_gameEngine->count();
    const int target = m_gameEngine->targetCount();
    const QString reason = (count > target)
        ? QString("Pressed %1 — too many! (target: %2)").arg(count).arg(target)
        : QString("Pressed %1 / %2 — too few!").arg(count).arg(target);

    if (m_btnStatusLabel) {
        m_btnStatusLabel->setText(reason + "\nRestarting...");
        m_btnStatusLabel->setStyleSheet(
            "QLabel { font-size: 14px; font-weight: 600; color: #ff6666; }");
    }
    if (m_btnCountdownLabel)
        m_btnCountdownLabel->setText("-");

    QTimer::singleShot(1500, this, [this]() {
        if (!m_gameEngine) return;
        m_gameEngine->restartGame();
        if (m_btnStatusLabel) {
            m_btnStatusLabel->setText("Press the physical button!");
            m_btnStatusLabel->setStyleSheet(
                "QLabel { font-size: 15px; font-weight: 600; color: #2d7dff; }");
        }
    });
}

// ── Ultrasonic dismiss UI ─────────────────────────────────────────────────────
//
// Corner layout (sensor index 0-based → shown as 1-4 to the user):
//
//   [Sensor 1] ─── [Sensor 2]        (top-left, top-right)
//       │                 │
//   [Sensor 3] ─── [Sensor 4]        (bottom-left, bottom-right)
//
// The screen shows a random 4-step sequence. The user holds a hand near the
// indicated sensor for 0.5 s. If the same sensor appears consecutively, the
// user must first remove their hand (distance > FAR_CM) and bring it back.

void DismissDialog::buildUltrasonicUi(const QStringList &alarmTimes)
{
    setFixedSize(560, 500);

    // ── Generate random 4-step sequence (values 0-3, repetitions allowed) ──
    QList<int> pool = { 0, 1, 2, 3 };
    for (int i = pool.size() - 1; i > 0; --i) {
        int j = static_cast<int>(QRandomGenerator::global()->bounded(static_cast<quint32>(i + 1)));
        pool.swapItemsAt(i, j);
    }
    for (int i = 0; i < 4; ++i)
        m_usSeq[i] = pool.at(i);

    m_usStep        = 0;
    m_usNeedRelease = false;
    m_usHolding     = false;

    // ── Layout ──────────────────────────────────────────────────────────────
    QVBoxLayout *root = new QVBoxLayout(this);
    root->setContentsMargins(20, 14, 20, 14);
    root->setSpacing(6);

    QLabel *titleLabel = new QLabel("Alarm is ringing!", this);
    titleLabel->setAlignment(Qt::AlignCenter);
    titleLabel->setStyleSheet("QLabel { font-size: 20px; font-weight: 700; color: #ff6666; }");
    root->addWidget(titleLabel);

    QLabel *timesLabel = new QLabel(alarmTimes.join("  /  "), this);
    timesLabel->setAlignment(Qt::AlignCenter);
    timesLabel->setStyleSheet("QLabel { font-size: 13px; color: #aaaaaa; }");
    root->addWidget(timesLabel);

    QLabel *instrLabel = new QLabel(
        "Hold hand near the sensor shown (0.5 s). Same sensor twice = remove hand first.", this);
    instrLabel->setAlignment(Qt::AlignCenter);
    instrLabel->setWordWrap(true);
    instrLabel->setStyleSheet("QLabel { font-size: 12px; color: #888888; }");
    root->addWidget(instrLabel);

    // ── Sequence boxes ───────────────────────────────────────────────────────
    QHBoxLayout *seqRow = new QHBoxLayout();
    seqRow->setSpacing(10);
    seqRow->addStretch();
    for (int i = 0; i < 4; ++i) {
        QLabel *lbl = new QLabel(QString::number(m_usSeq[i] + 1), this);
        lbl->setAlignment(Qt::AlignCenter);
        lbl->setFixedSize(60, 48);
        lbl->setStyleSheet(
            "QLabel { font-size: 22px; font-weight: 800; color: #cccccc;"
            "  background: #2c2c2c; border: 1px solid #555; border-radius: 10px; }");
        m_usStepLabels[i] = lbl;
        seqRow->addWidget(lbl);
    }
    seqRow->addStretch();
    root->addLayout(seqRow);

    // ── Big sensor number ────────────────────────────────────────────────────
    m_usTargetLabel = new QLabel(QString::number(m_usSeq[0] + 1), this);
    m_usTargetLabel->setAlignment(Qt::AlignCenter);
    m_usTargetLabel->setFixedHeight(110);
    m_usTargetLabel->setStyleSheet(
        "QLabel { font-size: 80px; font-weight: 900; color: #2d7dff; }");
    root->addWidget(m_usTargetLabel);

    // ── Progress label ───────────────────────────────────────────────────────
    m_usProgressLabel = new QLabel("Step 1 / 4", this);
    m_usProgressLabel->setAlignment(Qt::AlignCenter);
    m_usProgressLabel->setStyleSheet(
        "QLabel { font-size: 16px; font-weight: 700; color: #cccccc; }");
    root->addWidget(m_usProgressLabel);

    // ── Status/instruction label ─────────────────────────────────────────────
    m_usStatusLabel = new QLabel(
        QString("Bring hand near Sensor %1").arg(m_usSeq[0] + 1), this);
    m_usStatusLabel->setAlignment(Qt::AlignCenter);
    m_usStatusLabel->setWordWrap(true);
    m_usStatusLabel->setFixedHeight(44);
    m_usStatusLabel->setStyleSheet(
        "QLabel { font-size: 15px; font-weight: 600; color: #2d7dff; }");
    root->addWidget(m_usStatusLabel);

    // ── Corner layout diagram ────────────────────────────────────────────────
    QLabel *diagramLabel = new QLabel(
        "[1] ─── [2]\n │         │\n[3] ─── [4]", this);
    diagramLabel->setAlignment(Qt::AlignCenter);
    diagramLabel->setStyleSheet(
        "QLabel { font-size: 13px; font-family: monospace; color: #666666; }");
    root->addWidget(diagramLabel);

    // ── Live distance readouts ───────────────────────────────────────────────
    QHBoxLayout *distRow = new QHBoxLayout();
    distRow->setSpacing(8);
    distRow->addStretch();
    for (int i = 0; i < 4; ++i) {
        QLabel *lbl = new QLabel(QString("S%1:--").arg(i + 1), this);
        lbl->setAlignment(Qt::AlignCenter);
        lbl->setFixedSize(78, 28);
        lbl->setStyleSheet(
            "QLabel { font-size: 12px; color: #888888;"
            "  background: #1e1e1e; border: 1px solid #444; border-radius: 6px; }");
        m_usDistLabels[i] = lbl;
        distRow->addWidget(lbl);
    }
    distRow->addStretch();
    root->addLayout(distRow);

    // Highlight step 0 immediately
    usAdvanceToStep(0);

    // ── Start the watcher ────────────────────────────────────────────────────
    m_usWatcher = new UltrasonicWatcher(this);
    connect(m_usWatcher, &UltrasonicWatcher::distancesRead,
            this, &DismissDialog::onUltrasonicDistances);
}

// Helper: update step-box highlighting and target/progress labels
void DismissDialog::usAdvanceToStep(int step)
{
    const QString activeStyle =
        "QLabel { font-size: 22px; font-weight: 800; color: #111111;"
        "  background: #2d7dff; border: 1px solid #3a8cff; border-radius: 10px; }";
    const QString doneStyle =
        "QLabel { font-size: 22px; font-weight: 800; color: #111111;"
        "  background: #46a446; border: 1px solid #55b755; border-radius: 10px; }";
    const QString pendingStyle =
        "QLabel { font-size: 22px; font-weight: 800; color: #cccccc;"
        "  background: #2c2c2c; border: 1px solid #555; border-radius: 10px; }";

    for (int i = 0; i < 4; ++i) {
        if (!m_usStepLabels[i]) continue;
        if (i < step)
            m_usStepLabels[i]->setStyleSheet(doneStyle);
        else if (i == step)
            m_usStepLabels[i]->setStyleSheet(activeStyle);
        else
            m_usStepLabels[i]->setStyleSheet(pendingStyle);
    }

    if (m_usTargetLabel)
        m_usTargetLabel->setText(QString::number(m_usSeq[step] + 1));
    if (m_usProgressLabel)
        m_usProgressLabel->setText(QString("Step %1 / 4").arg(step + 1));
}

// ── Ultrasonic state machine ──────────────────────────────────────────────────
void DismissDialog::onUltrasonicDistances(QVector<int> distances)
{
    static const int NEAR_CM = 15;   // hand is "near" when distance < 15 cm
    static const int FAR_CM  = 20;   // hand is "away"  when distance > 20 cm (hysteresis)
    static const int HOLD_MS = 500;  // must hold near for 500 ms

    if (m_usStep >= 4) return;

    // ── Update live distance labels ──────────────────────────────────────────
    for (int i = 0; i < 4; ++i) {
        if (!m_usDistLabels[i]) continue;
        const int d = (i < distances.size()) ? distances[i] : -1;
        m_usDistLabels[i]->setText(
            d < 0 ? QString("S%1:ERR").arg(i + 1)
                  : QString("S%1:%2cm").arg(i + 1).arg(d));
        m_usDistLabels[i]->setStyleSheet(
            (d > 0 && d < NEAR_CM)
                ? "QLabel { font-size:12px; color:#111; background:#2d7dff;"
                  "  border:1px solid #3a8cff; border-radius:6px; }"
                : "QLabel { font-size:12px; color:#888888; background:#1e1e1e;"
                  "  border:1px solid #444; border-radius:6px; }");
    }

    const int sensorIdx = m_usSeq[m_usStep];
    const int dist      = (sensorIdx < distances.size()) ? distances[sensorIdx] : -1;
    const bool isNear   = (dist > 0 && dist < NEAR_CM);
    const bool isFar    = (dist < 0 || dist >= FAR_CM);

    // ── Wait-for-release phase (same sensor repeated) ────────────────────────
    if (m_usNeedRelease) {
        if (isFar) {
            m_usNeedRelease = false;
            m_usHolding     = false;
            if (m_usStatusLabel)
                m_usStatusLabel->setText(
                    QString("Step %1/4  ·  Bring hand near Sensor %2")
                    .arg(m_usStep + 1).arg(sensorIdx + 1));
        } else {
            if (m_usStatusLabel)
                m_usStatusLabel->setText(
                    QString("Step %1/4  ·  Remove hand, then bring near Sensor %2")
                    .arg(m_usStep + 1).arg(sensorIdx + 1));
        }
        return;
    }

    // ── Hold phase ───────────────────────────────────────────────────────────
    if (isNear) {
        if (!m_usHolding) {
            // Start of a new hold
            m_usHolding = true;
            m_usHoldTimer.restart();
            if (m_usStatusLabel)
                m_usStatusLabel->setText(
                    QString("Step %1/4  ·  Hold Sensor %2 ... keep still!")
                    .arg(m_usStep + 1).arg(sensorIdx + 1));
        } else {
            const qint64 elapsed = m_usHoldTimer.elapsed();
            if (elapsed >= HOLD_MS) {
                // ── Step recognised! ────────────────────────────────────────
                if (m_usStepLabels[m_usStep]) {
                    m_usStepLabels[m_usStep]->setStyleSheet(
                        "QLabel { font-size:22px; font-weight:800; color:#111;"
                        "  background:#46a446; border:1px solid #55b755; border-radius:10px; }");
                }

                ++m_usStep;

                if (m_usStep >= 4) {
                    accept();
                    return;
                }

                // Prepare next step
                const bool sameAsPrev = (m_usSeq[m_usStep] == m_usSeq[m_usStep - 1]);
                m_usNeedRelease = sameAsPrev;
                m_usHolding     = false;

                usAdvanceToStep(m_usStep);

                const int nextSensor = m_usSeq[m_usStep];
                if (m_usStatusLabel) {
                    m_usStatusLabel->setText(
                        sameAsPrev
                        ? QString("Step %1/4  ·  Remove hand, then bring near Sensor %2")
                          .arg(m_usStep + 1).arg(nextSensor + 1)
                        : QString("Step %1/4  ·  Bring hand near Sensor %2")
                          .arg(m_usStep + 1).arg(nextSensor + 1));
                }
            } else {
                // Still building up hold — show percentage
                const int pct = static_cast<int>(elapsed * 100 / HOLD_MS);
                if (m_usStatusLabel)
                    m_usStatusLabel->setText(
                        QString("Step %1/4  ·  Holding %2%  keep still!")
                        .arg(m_usStep + 1).arg(pct));
            }
        }
    } else {
        // Hand moved away — reset hold
        if (m_usHolding) {
            m_usHolding = false;
            if (m_usStatusLabel)
                m_usStatusLabel->setText(
                    QString("Step %1/4  ·  Bring hand near Sensor %2")
                    .arg(m_usStep + 1).arg(sensorIdx + 1));
        }
    }
}
