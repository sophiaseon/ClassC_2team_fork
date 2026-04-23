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

void MainWindow::on_btnPush_clicked()
{
    qDebug() << "btnPush clicked";
}

void MainWindow::on_btnDisabled_clicked()
{
    qDebug() << "btnPush clicked";
}

void MainWindow::on_btnFlat_clicked()
{
    qDebug() << "btnFlat clicked";
}

void MainWindow::on_btnCheckable_clicked()
{
    qDebug() << "btnCheckable clicked";
}

void MainWindow::on_btnAutoRepeat_clicked()
{
    qDebug() << "btnAutoRepeat clicked";
}
