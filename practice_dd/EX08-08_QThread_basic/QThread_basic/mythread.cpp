#include "mythread.h"

MyThread::MyThread(QObject *parent)
    : QThread{parent}
{
    running = false;
}

void MyThread::run()
{
    int cnt = 0;
    running = true;
    while(running == true) {
        msleep(100);
        if(cnt++ == 10) {
            cnt = 0;
            emit send_command(1);
        }
    }
}

void MyThread::stop()
{
    running = false;
}

bool MyThread::is_running()
{
    return running;
}
