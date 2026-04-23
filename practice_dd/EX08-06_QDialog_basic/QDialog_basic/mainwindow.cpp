#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::on_btnExec_clicked()
{
    MyDialog *dlg = new MyDialog(this);
    int r = dlg->exec();
    qDebug() << "exec return" << r;
}

void MainWindow::on_btnOpen_clicked()
{
    MyDialog *dlg = new MyDialog(this);
    dlg->open();
    qDebug() << "open return";
}

void MainWindow::on_btnShow_clicked()
{
    MyDialog *dlg = new MyDialog(this);
    dlg->show();
    qDebug() << "show return";
}
