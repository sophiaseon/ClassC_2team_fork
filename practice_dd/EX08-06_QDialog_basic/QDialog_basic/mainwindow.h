#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QDebug>
#include "mydialog.h"

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
    void on_btnExec_clicked();

    void on_btnOpen_clicked();

    void on_btnShow_clicked();

private:
    Ui::MainWindow *ui;
};

#endif // MAINWINDOW_H
