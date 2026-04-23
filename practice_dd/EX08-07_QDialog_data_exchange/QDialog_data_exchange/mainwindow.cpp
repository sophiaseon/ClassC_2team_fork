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

void MainWindow::receive_hobby(QString str)
{
    qDebug() << "receive_hobby" << str;
    ui->lblHobby->setText(str);
}

void MainWindow::on_btnEdit_clicked()
{
    MyDialog *dlg = new MyDialog(this);
    dlg->open();
    qDebug() << "open return";
}
