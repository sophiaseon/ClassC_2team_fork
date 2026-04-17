#ifndef STATDIALOG_H
#define STATDIALOG_H

#include <QDialog>
#include <QElapsedTimer>

class QListWidget;

class StatDialog : public QDialog
{
    Q_OBJECT

public:
    explicit StatDialog(const QString &logFilePath, QWidget *parent = nullptr,
                        const QString &title = QString());

private:
    void buildUi(const QString &logFilePath, const QString &title);

    QElapsedTimer m_actionTimer;
};

#endif // STATDIALOG_H
