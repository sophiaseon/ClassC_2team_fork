#include "mydialog.h"
#include "ui_mydialog.h"

MyDialog::MyDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::MyDialog)
{
    ui->setupUi(this);

    connect(this, SIGNAL(accepted()), this, SLOT(dlg_accepted()));
    connect(this, SIGNAL(rejected()), this, SLOT(dlg_rejected()));
    connect(this, SIGNAL(finished(int)), this, SLOT(dlg_finished(int)));
}

MyDialog::~MyDialog()
{
    delete ui;
}

void MyDialog::dlg_accepted()
{
    qDebug() << "accepted";
}


void MyDialog::dlg_rejected()
{
    qDebug() << "rejected";
}


void MyDialog::dlg_finished(int result)
{
    qDebug() << "finished" << result;
}


void MyDialog::on_btnAccept_clicked()
{
    accept();
}

void MyDialog::on_btnReject_clicked()
{
    reject();
}

void MyDialog::on_btnDone_clicked()
{
    done(10);
}
