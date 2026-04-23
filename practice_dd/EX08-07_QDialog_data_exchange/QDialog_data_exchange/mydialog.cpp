#include "mydialog.h"
#include "ui_mydialog.h"

MyDialog::MyDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::MyDialog)
{
    ui->setupUi(this);
    connect(this, SIGNAL(send_hobby(QString)), parent, SLOT(receive_hobby(QString)));
}

MyDialog::~MyDialog()
{
    delete ui;
}

void MyDialog::on_btnOK_clicked()
{
    QString str = QString("Hobby:");
    int one_or_more = 0;
    if(ui->chkFishing->isChecked())
    {
        one_or_more++;
        str.append(" Fishing");
    }
    if(ui->chkSurfing->isChecked())
    {
        one_or_more++;
        str.append(" Surfing");
    }
    if(ui->chkTraveling->isChecked())
    {
        one_or_more++;
        str.append(" Traveling");
    }
    if(one_or_more == 0)
    {
        str.append(" None");
    }

    emit send_hobby(str);
    accept();
}

void MyDialog::on_btnCancel_clicked()
{
    reject();
}
