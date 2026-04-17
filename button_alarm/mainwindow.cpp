//#include "mainwindow.h"
//#include "ui_mainwindow.h"

//MainWindow::MainWindow(QWidget *parent)
//    : QMainWindow(parent)
//    , ui(new Ui::MainWindow)
//{
//    ui->setupUi(this);
//}

//MainWindow::~MainWindow()
//{
//    delete ui;
//}


#include "mainwindow.h"

#include <QVBoxLayout>
#include <QKeyEvent>
#include <QDebug>
#include <QMessageBox>

// 페이지 인덱스 상수
static constexpr int PAGE_READY   = 0;
static constexpr int PAGE_PLAYING = 1;
static constexpr int PAGE_SUCCESS = 2;
static constexpr int PAGE_FAILURE = 3;

// -------------------------------------------------------
// 생성자
// -------------------------------------------------------
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle("30 times Button Press Game");
    setFixedSize(1024, 600);    // PRD 해상도 고정

    applyGlobalStyle();

    // --- 모듈 생성 ---
    m_engine = new GameEngine(this);
    m_button = new ButtonHandler("/dev/kmsg", 50, this);

    // --- 화면 구성 ---
    m_stack = new QStackedWidget(this);
    m_readyPage   = buildReadyPage();
    m_playPage    = buildPlayingPage();
    m_successPage = buildSuccessPage();
    m_failurePage = buildFailurePage();

    m_stack->addWidget(m_readyPage);    // index 0
    m_stack->addWidget(m_playPage);     // index 1
    m_stack->addWidget(m_successPage);  // index 2
    m_stack->addWidget(m_failurePage);  // index 3
    setCentralWidget(m_stack);

    connect(m_button,  &ButtonHandler::buttonPressed,
            m_engine,  &GameEngine::onButtonPressed);
    connect(m_button,  &ButtonHandler::deviceError,
            this,      &MainWindow::onDeviceError);

    connect(m_engine,  &GameEngine::stateChanged,
            this,      &MainWindow::onStateChanged);
    connect(m_engine,  &GameEngine::countUpdated,
            this,      &MainWindow::onCountUpdated);
    connect(m_engine,  &GameEngine::countdownUpdated,
            this,      &MainWindow::onCountdownUpdated);

    m_button->open();
    showPage(PAGE_READY);
}

// -------------------------------------------------------
// 스타일
// -------------------------------------------------------
void MainWindow::applyGlobalStyle()
{
    setStyleSheet(R"(
        QMainWindow, QWidget {
            background-color: #1a1a2e;
            color: #e0e0e0;
        }
        QLabel#titleLabel {
            font-size: 60px;
            font-weight: bold;
            color: #e94560;
        }
        QLabel#subLabel {
            font-size: 28px;
            color: #a0a0c0;
        }
        QLabel#countLabel {
            font-size: 150px;
            font-weight: bold;
            color: #e94560;
        }
        QLabel#countdownLabel {
            font-size: 72px;
            font-weight: bold;
            color: #ffffff;
        }
        QLabel#progressLabel {
            font-size: 36px;
            color: #a0a0c0;
        }
        QLabel#successLabel {
            font-size: 72px;
            font-weight: bold;
            color: #4caf50;
        }
        QLabel#successSub {
            font-size: 36px;
            color: #a0a0c0;
        }
        QLabel#failureLabel {
            font-size: 72px;
            font-weight: bold;
            color: #e94560;
        }
        QLabel#failureSub {
            font-size: 36px;
            color: #a0a0c0;
        }
        QPushButton {
            background-color: #e94560;
            color: #ffffff;
            border: none;
            border-radius: 12px;
            font-size: 32px;
            padding: 16px 48px;
        }
        QPushButton:hover {
            background-color: #c73652;
        }
        QPushButton:pressed {
            background-color: #a02840;
        }
    )");
}

// -------------------------------------------------------
// Ready 페이지
// -------------------------------------------------------
QWidget *MainWindow::buildReadyPage()
{
    auto *page   = new QWidget(this);
    auto *layout = new QVBoxLayout(page);
    layout->setAlignment(Qt::AlignCenter);
    layout->setSpacing(30);

    auto *title = new QLabel("30 times Button Press Game", page);
    title->setObjectName("titleLabel");
    title->setAlignment(Qt::AlignCenter);

    auto *sub = new QLabel("Press the START GAME button to start\n", page);
    sub->setObjectName("subLabel");
    sub->setAlignment(Qt::AlignCenter);

    auto *startBtn = new QPushButton("START GAME", page);
    connect(startBtn, &QPushButton::clicked, m_engine, &GameEngine::startGame);

    layout->addWidget(title);
    layout->addWidget(sub);
    layout->addSpacing(20);
    layout->addWidget(startBtn, 0, Qt::AlignCenter);

    return page;
}

// -------------------------------------------------------
// Playing 페이지
// -------------------------------------------------------
QWidget *MainWindow::buildPlayingPage()
{
    auto *page   = new QWidget(this);
    auto *layout = new QVBoxLayout(page);
    layout->setAlignment(Qt::AlignCenter);
    layout->setSpacing(10);

    auto *hintLabel = new QLabel("Press the SW3 button rapidly!", page);
    hintLabel->setObjectName("subLabel");
    hintLabel->setAlignment(Qt::AlignCenter);

    m_countdownLabel = new QLabel(QString::number(GameEngine::GAME_DURATION), page);
    m_countdownLabel->setObjectName("countdownLabel");
    m_countdownLabel->setAlignment(Qt::AlignCenter);

    m_countLabel = new QLabel("0", page);
    m_countLabel->setObjectName("countLabel");
    m_countLabel->setAlignment(Qt::AlignCenter);

    m_progressLabel = new QLabel("0 / ?", page);
    m_progressLabel->setObjectName("progressLabel");
    m_progressLabel->setAlignment(Qt::AlignCenter);

    layout->addWidget(hintLabel);
    layout->addWidget(m_countdownLabel);
    layout->addWidget(m_countLabel);
    layout->addWidget(m_progressLabel);

    return page;
}

// -------------------------------------------------------
// Result 페이지
// -------------------------------------------------------
QWidget *MainWindow::buildSuccessPage()
{
    auto *page   = new QWidget(this);
    auto *layout = new QVBoxLayout(page);
    layout->setAlignment(Qt::AlignCenter);
    layout->setSpacing(30);

    auto *title = new QLabel("SUCCESS!", page);
    title->setObjectName("successLabel");
    title->setAlignment(Qt::AlignCenter);

    m_successResultLabel = new QLabel("0 / 0", page);
    m_successResultLabel->setObjectName("successSub");
    m_successResultLabel->setAlignment(Qt::AlignCenter);

    layout->addWidget(title);
    layout->addWidget(m_successResultLabel);

    return page;
}

QWidget *MainWindow::buildFailurePage()
{
    auto *page   = new QWidget(this);
    auto *layout = new QVBoxLayout(page);
    layout->setAlignment(Qt::AlignCenter);
    layout->setSpacing(30);

    auto *title = new QLabel("FAILED!", page);
    title->setObjectName("failureLabel");
    title->setAlignment(Qt::AlignCenter);

    m_failureResultLabel = new QLabel("0 / 0", page);
    m_failureResultLabel->setObjectName("failureSub");
    m_failureResultLabel->setAlignment(Qt::AlignCenter);

    auto *retryBtn = new QPushButton("Try Again", page);
    connect(retryBtn, &QPushButton::clicked, m_engine, &GameEngine::restartGame);

    layout->addWidget(title);
    layout->addWidget(m_failureResultLabel);
    layout->addSpacing(20);
    layout->addWidget(retryBtn, 0, Qt::AlignCenter);

    return page;
}

// -------------------------------------------------------
// 유틸리티
// -------------------------------------------------------
void MainWindow::showPage(int index)
{
    m_stack->setCurrentIndex(index);
}

// -------------------------------------------------------
// 키보드 폴백 (비-Linux 개발 환경 또는 터치 보조)
// -------------------------------------------------------
void MainWindow::keyPressEvent(QKeyEvent *event)
{
    switch (event->key()) {
    case Qt::Key_Space:
    case Qt::Key_Return:
    case Qt::Key_Enter:
        // READY 상태라면 게임 시작, PLAYING 이면 버튼 입력으로 처리
        if (m_engine->state() == GameEngine::State::READY) {
            m_engine->startGame();
        } else if (m_engine->state() == GameEngine::State::PLAYING) {
            // 키보드 자동 반복은 물리 버튼이 아니므로 isAutoRepeat 체크
            if (!event->isAutoRepeat()) {
                m_engine->onButtonPressed();
            }
        } else if (m_engine->state() == GameEngine::State::SUCCESS ||
                   m_engine->state() == GameEngine::State::FAILURE) {
            m_engine->restartGame();
        }
        break;
    default:
        QMainWindow::keyPressEvent(event);
        break;
    }
}

// -------------------------------------------------------
// 슬롯: 상태 변화
// -------------------------------------------------------
void MainWindow::onStateChanged(GameEngine::State state)
{
    switch (state) {
    case GameEngine::State::READY:
        showPage(PAGE_READY);
        break;
    case GameEngine::State::PLAYING:
        m_countLabel->setText("0");
        m_progressLabel->setText(QString("0 / %1").arg(m_engine->targetCount()));
        m_countdownLabel->setText(QString::number(GameEngine::GAME_DURATION));
        showPage(PAGE_PLAYING);
        break;
    case GameEngine::State::SUCCESS:
        m_successResultLabel->setText(
            QString("%1 / %2").arg(m_engine->count()).arg(m_engine->targetCount()));
        showPage(PAGE_SUCCESS);
        break;
    case GameEngine::State::FAILURE:
        m_failureResultLabel->setText(
            QString("%1 / %2").arg(m_engine->count()).arg(m_engine->targetCount()));
        showPage(PAGE_FAILURE);
        break;
    }
}

// -------------------------------------------------------
// 슬롯: 카운트 갱신
// -------------------------------------------------------
void MainWindow::onCountUpdated(int count)
{
    m_countLabel->setText(QString::number(count));
    m_progressLabel->setText(QString("%1 / %2").arg(count).arg(m_engine->targetCount()));
}

void MainWindow::onCountdownUpdated(int secondsLeft)
{
    m_countdownLabel->setText(QString::number(secondsLeft));
}

// -------------------------------------------------------
// 슬롯: 디바이스 에러
// -------------------------------------------------------
void MainWindow::onDeviceError(const QString &message)
{
    qWarning() << "ButtonHandler error:" << message;
    QMessageBox::warning(this, "Input Device Error",
                         message + "\n\nYou can continue using the keyboard (Space/Enter).");
}
