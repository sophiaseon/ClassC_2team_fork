#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <csignal>
#include <sys/socket.h>
#include <unistd.h>
#include <QApplication>

int MainWindow::sigFd[2] = {-1, -1};

static void sigHandler(int)
{
    char a = 1;
    (void)::write(MainWindow::sigFd[0], &a, sizeof(a));
}

void MainWindow::setupSignalHandlers()
{
    // self-pipe trick: 신호를 Qt 이벤트 루프에 안전하게 전달
    ::socketpair(AF_UNIX, SOCK_STREAM, 0, sigFd);
    struct sigaction sa;
    sa.sa_handler = sigHandler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sigaction(SIGTERM, &sa, nullptr);
    sigaction(SIGINT,  &sa, nullptr);
}

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    setupSignalHandlers();

    // QSocketNotifier로 pipe 읽기 감지 → Qt 이벤트 루프 안에서 안전하게 종료
    snTerm = new QSocketNotifier(sigFd[1], QSocketNotifier::Read, this);
    connect(snTerm, SIGNAL(activated(int)), this, SLOT(handleSigTerm()));

    camera = new CameraThread(this);
    connect(camera, SIGNAL(send_image(QImage)), this, SLOT(handle_image(QImage)));
    camera->start();
}
MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::handleSigTerm()
{
    snTerm->setEnabled(false);
    char tmp;
    (void)::read(sigFd[1], &tmp, sizeof(tmp));
    close(); // closeEvent 호출 → 카메라 정리 후 종료
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    // 앱 종료 시 카메라 스레드를 안전하게 멈추고 UVC 드라이버 정리
    camera->requestStop();
    camera->wait(3000); // 최대 3초 대기
    QMainWindow::closeEvent(event);
}

void MainWindow::handle_image(QImage image)
{
    ui->lblImg->setPixmap(QPixmap::fromImage(image));
    // 렌더링 완료 후 카메라 스레드에 신호 - 다음 프레임 받을 준비
    camera->m_uiReady = 1;
}
