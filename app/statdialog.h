#ifndef STATDIALOG_H
#define STATDIALOG_H

#include <QDialog>
#include <QElapsedTimer>
#include <QDate>
#include <QList>
#include <QMap>
#include <QTime>

class CalendarWidget;
class QLabel;
class QListWidget;
class QStackedWidget;

class StatDialog : public QDialog
{
    Q_OBJECT

public:
    // Local mode: reads log from a file on this device
    explicit StatDialog(const QString &logFilePath, QWidget *parent = nullptr,
                        const QString &title = QString());
    // Friend mode: log data already fetched over the network
    explicit StatDialog(const QByteArray &logData, const QString &friendIp,
                        QWidget *parent, const QString &title = QString());

private:
    void       buildUi();
    void       populateList(const QByteArray &logData);
    QByteArray fetchFromFriend(const QString &command) const;

    QString          m_title;
    QString          m_friendIp;                // empty = local mode
    QListWidget     *m_listWidget    = nullptr;
    QLabel          *m_summaryLabel  = nullptr;
    QStackedWidget  *m_stack         = nullptr;
    CalendarWidget  *m_chartWidget   = nullptr;
    QElapsedTimer    m_actionTimer;
};

#endif // STATDIALOG_H
