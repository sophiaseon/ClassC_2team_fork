#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    image_buf = new uchar[640 * 480 * 3];
    camera = new CameraThread(this);
    connect(camera, SIGNAL(send_data(const uchar*,int,int)), this, SLOT(handle_data(const uchar*,int,int)));
    camera->start();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::handle_data(const uchar *data, int width, int height)
{
    //qDebug() << "handle_data" << data << width << height;
    yuyv2rgb(data, width, height, image_buf);
    QPixmap pixmap = QPixmap::fromImage(QImage(image_buf, width, height, QImage::Format_RGB888));
    ui->lblImg->setPixmap(pixmap);
}

bool MainWindow::yuyv2rgb(const uchar *yuyv, int width, int height, uchar *rgb)
{
    long yuv_size = height * width * 2;
    long rgb_size = height * width * 3;

    if (yuyv == NULL || rgb == NULL)
    return false;

    for (int i = 0, j = 0; i < rgb_size && j < yuv_size; i += 6, j += 4)
    {
    yuyv_to_rgb_pixel(&yuyv[j], &rgb[i]);
    }
    return true;
}

void MainWindow::yuyv_to_rgb_pixel(const uchar *yuyv, uchar *rgb)
{
    int y, v, u;
    float r, g, b;

    y = yuyv[0]; //y0
    u = yuyv[1]; //u0
    v = yuyv[3]; //v0

    r = y + 1.4065 * (v - 128);			     //r0
    g = y - 0.3455 * (u - 128) - 0.7169 * (v - 128); //g0
    b = y + 1.1790 * (u - 128);			     //b0

    if (r < 0)
    r = 0;
    else if (r > 255)
    r = 255;
    if (g < 0)
    g = 0;
    else if (g > 255)
    g = 255;
    if (b < 0)
    b = 0;
    else if (b > 255)
    b = 255;

    rgb[0] = (unsigned char)r;
    rgb[1] = (unsigned char)g;
    rgb[2] = (unsigned char)b;

    //second pixel
    u = yuyv[1]; //u0
    y = yuyv[2]; //y1
    v = yuyv[3]; //v0

    r = y + 1.4065 * (v - 128);			     //r1
    g = y - 0.3455 * (u - 128) - 0.7169 * (v - 128); //g1
    b = y + 1.1790 * (u - 128);			     //b1

    if (r < 0)
    r = 0;
    else if (r > 255)
    r = 255;
    if (g < 0)
    g = 0;
    else if (g > 255)
    g = 255;
    if (b < 0)
    b = 0;
    else if (b > 255)
    b = 255;

    rgb[3] = (unsigned char)r;
    rgb[4] = (unsigned char)g;
    rgb[5] = (unsigned char)b;
}
