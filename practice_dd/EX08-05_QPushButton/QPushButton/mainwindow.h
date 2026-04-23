#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QDebug>

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
    void on_btnPush_clicked();

    void on_btnDisabled_clicked();

    void on_btnFlat_clicked();

    void on_btnCheckable_clicked();

    void on_btnAutoRepeat_clicked();

private:
    Ui::MainWindow *ui;
};

#endif // MAINWINDOW_H
