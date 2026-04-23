#ifndef MYDIALOG_H
#define MYDIALOG_H

#include <QDialog>
#include <QDebug>

namespace Ui {
class MyDialog;
}

class MyDialog : public QDialog
{
    Q_OBJECT

public:
    explicit MyDialog(QWidget *parent = 0);
    ~MyDialog();

signals:
    void send_hobby(QString);

private slots:
    void on_btnOK_clicked();

    void on_btnCancel_clicked();

private:
    Ui::MyDialog *ui;
};

#endif // MYDIALOG_H
