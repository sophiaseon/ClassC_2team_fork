#ifndef STATDIALOG_H
#define STATDIALOG_H

#include <QDialog>

class QListWidget;

class StatDialog : public QDialog
{
    Q_OBJECT

public:
    explicit StatDialog(const QString &logFilePath, QWidget *parent = nullptr);

private:
    void buildUi(const QString &logFilePath);
};

#endif // STATDIALOG_H
