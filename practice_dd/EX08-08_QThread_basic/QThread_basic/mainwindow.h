#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QDebug>
#include "mythread.h"

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

private slots:
    void on_btnStart_clicked();
    void on_btnStop_clicked();
    void handle_command(int cmd);

private:
    Ui::MainWindow *ui;
    MyThread *myThread;
};

#endif // MAINWINDOW_H
