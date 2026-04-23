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

private slots:
    void dlg_accepted();
    void dlg_rejected();
    void dlg_finished(int);
    void on_btnAccept_clicked();
    void on_btnReject_clicked();
    void on_btnDone_clicked();

private:
    Ui::MyDialog *ui;
};

#endif // MYDIALOG_H
