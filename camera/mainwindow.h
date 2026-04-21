#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QDebug>
#include <QByteArray>
#include <QCloseEvent>
#include <QSocketNotifier>
#include "camerathread.h"

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();
    static void setupSignalHandlers();
    static int sigFd[2];

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void handleSigTerm();

public slots:
    void handle_image(QImage image);

private:
    Ui::MainWindow *ui;
    CameraThread *camera;
    int front_index;
    QSocketNotifier *snTerm;
};

#endif // MAINWINDOW_H
