#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QDebug>
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

public slots:
    void handle_data(const uchar *data, int width, int height);

private:
    Ui::MainWindow *ui;
    CameraThread *camera;
    bool yuyv2rgb(const uchar *yuyv, int width, int height, uchar *rgb);
    void yuyv_to_rgb_pixel(const uchar *yuyv, uchar *rgb);
    uchar *image_buf;
    int front_index;
};

#endif // MAINWINDOW_H
