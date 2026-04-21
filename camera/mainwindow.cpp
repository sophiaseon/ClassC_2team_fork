#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <csignal>
#include <QApplication>

static MainWindow *g_mainWindow = nullptr;

static void signalHandler(int)
{
    // SIGINT(Ctrl+C) / SIGTERM 수신 시 카메라 정리 후 종료
    if (g_mainWindow)
        g_mainWindow->close();
    else
        QApplication::quit();
}

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    g_mainWindow = this;
    std::signal(SIGINT,  signalHandler);
    std::signal(SIGTERM, signalHandler);

    camera = new CameraThread(this);
    connect(camera, SIGNAL(send_image(QImage)), this, SLOT(handle_image(QImage)));
    camera->start();
}
MainWindow::~MainWindow()
{
    delete ui;
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
