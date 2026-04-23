#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    myThread = new MyThread(this);
    connect(myThread, SIGNAL(send_command(int)), this, SLOT(handle_command(int)));
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::handle_command(int cmd)
{
    qDebug() << "handle_command" << cmd;
    if(cmd == 1) {
        static int toggle = 0;
        toggle ^= 1;
        if(toggle) {
            ui->lblRedLED->setStyleSheet("background-color: red");
        }
        else {
            ui->lblRedLED->setStyleSheet("background-color: black");
        }
    }
}

void MainWindow::on_btnStart_clicked()
{
    qDebug() << "Start";
    if(myThread->is_running()) {
        qDebug() << "already started";
        return;
    }
    myThread->start();
}

void MainWindow::on_btnStop_clicked()
{
    qDebug() << "Stop";
    myThread->stop();
}
