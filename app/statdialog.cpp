#include "statdialog.h"

#include <QEventLoop>
#include <QFile>
#include <QHBoxLayout>
#include <QImage>
#include <QImageReader>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPixmap>
#include <QPushButton>
#include <QTcpSocket>
#include <QTextStream>
#include <QTimer>
#include <QVBoxLayout>

static const quint16 ALARM_PORT    = 45678;
static const int     FETCH_TIMEOUT = 5000;

// ── Local-mode constructor ────────────────────────────────────────────────────
StatDialog::StatDialog(const QString &logFilePath, QWidget *parent, const QString &title)
    : QDialog(parent)
    , m_title(title)
{
    m_actionTimer.start();
    buildUi();

    QByteArray data;
    QFile f(logFilePath);
    if (f.open(QIODevice::ReadOnly))
        data = f.readAll();
    populateList(data);
}

// ── Friend-mode constructor ───────────────────────────────────────────────────
StatDialog::StatDialog(const QByteArray &logData, const QString &friendIp,
                       QWidget *parent, const QString &title)
    : QDialog(parent)
    , m_title(title)
    , m_friendIp(friendIp)
{
    m_actionTimer.start();
    buildUi();
    populateList(logData);
}

// ── Network helper (nested QEventLoop — safe inside dlg.exec()) ───────────────
QByteArray StatDialog::fetchFromFriend(const QString &command) const
{
    QTcpSocket sock;
    QEventLoop loop;
    QTimer     timer;
    timer.setSingleShot(true);

    bool ok = false;
    QObject::connect(&sock,  &QTcpSocket::connected,    &loop, [&]() { ok = true; loop.quit(); });
    QObject::connect(&sock,  &QTcpSocket::disconnected, &loop, &QEventLoop::quit);
    QObject::connect(&timer, &QTimer::timeout,          &loop, &QEventLoop::quit);

    timer.start(FETCH_TIMEOUT);
    sock.connectToHost(m_friendIp, ALARM_PORT);
    loop.exec();
    timer.stop();
    if (!ok) return {};

    sock.write((command + "\n").toUtf8());
    sock.flush();

    QByteArray data;
    QObject::connect(&sock, &QTcpSocket::readyRead, &sock, [&]() { data += sock.readAll(); });
    data += sock.readAll();
    timer.start(FETCH_TIMEOUT);
    if (sock.state() != QAbstractSocket::UnconnectedState)
        loop.exec();
    timer.stop();
    data += sock.readAll();
    sock.disconnectFromHost();
    return data;
}

// ── Build UI chrome (no list content) ───────────────────────────────────────
void StatDialog::buildUi()
{
    const QString displayTitle = m_title.isEmpty() ? "Alarm Dismiss History" : m_title;
    setWindowTitle(m_title.isEmpty() ? "Alarm Statistics" : m_title);
    setFixedSize(780, 500);
    setStyleSheet("QDialog { background: #0b0b0b; border: 2px solid #ffffff; border-radius: 4px; }");

    QVBoxLayout *root = new QVBoxLayout(this);
    root->setContentsMargins(20, 16, 20, 16);
    root->setSpacing(12);

    QLabel *titleLabel = new QLabel(displayTitle, this);
    titleLabel->setAlignment(Qt::AlignCenter);
    titleLabel->setStyleSheet(
        "QLabel { font-size: 19px; font-weight: 700; color: white; }"
    );
    root->addWidget(titleLabel);

    // Header row
    QWidget *headerWidget = new QWidget(this);
    headerWidget->setStyleSheet("background: #1e1e1e; border-radius: 6px;");
    QHBoxLayout *headerLayout = new QHBoxLayout(headerWidget);
    headerLayout->setContentsMargins(14, 8, 14, 8);
    headerLayout->setSpacing(0);

    auto makeHeaderLabel = [headerWidget](const QString &text) -> QLabel * {
        QLabel *lbl = new QLabel(text, headerWidget);
        lbl->setStyleSheet(
            "QLabel { font-size: 13px; font-weight: 700; color: #aaaaaa; background: transparent; }"
        );
        lbl->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        return lbl;
    };

    QLabel *h1 = makeHeaderLabel("#");
    QLabel *h2 = makeHeaderLabel("Alarm Time");
    QLabel *h3 = makeHeaderLabel("Dismissed At");
    QLabel *h4 = makeHeaderLabel("Photo");
    h1->setFixedWidth(32);
    h4->setAlignment(Qt::AlignCenter);
    h4->setFixedWidth(120);

    headerLayout->addWidget(h1);
    headerLayout->addWidget(h2, 5);
    headerLayout->addWidget(h3, 5);
    headerLayout->addWidget(h4);
    root->addWidget(headerWidget);

    // List widget — stored as member so populateList() can refresh it
    m_listWidget = new QListWidget(this);
    m_listWidget->setStyleSheet(
        "QListWidget {"
        "    background: #141414; border: 1px solid #3a3a3a;"
        "    border-radius: 8px; outline: none; padding: 4px;"
        "}"
        "QListWidget::item { padding: 2px 6px; border-radius: 4px; }"
        "QListWidget::item:alternate { background: #1a1a1a; }"
        "QListWidget::item:selected { background: #2d7dff; }"
        /* scrollbar */
        "QScrollBar:vertical {"
        "    background: #141414; width: 8px;"
        "    margin: 6px 2px 6px 2px; border-radius: 4px;"
        "}"
        "QScrollBar::handle:vertical {"
        "    background: #3a3a3a; border-radius: 4px; min-height: 24px;"
        "}"
        "QScrollBar::handle:vertical:hover { background: #555555; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0px; }"
        "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: none; }"
    );
    m_listWidget->setAlternatingRowColors(true);
    m_listWidget->setSelectionMode(QAbstractItemView::NoSelection);

    // Summary label — stored as member so populateList() can update it
    m_summaryLabel = new QLabel(this);
    m_summaryLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_summaryLabel->setStyleSheet("QLabel { font-size: 12px; color: #888888; }");

    // Bottom button row
    QHBoxLayout *bottomRow = new QHBoxLayout();
    bottomRow->setSpacing(10);

    auto makeBtn = [this](const QString &label, const QString &bg, const QString &bgPress) {
        QPushButton *btn = new QPushButton(label, this);
        btn->setFixedHeight(44);
        btn->setStyleSheet(
            QString("QPushButton { font-size: 16px; font-weight: 700; color: white;"
                    "    background: %1; border: none; border-radius: 10px; }"
                    "QPushButton:pressed { background: %2; }").arg(bg, bgPress)
        );
        return btn;
    };

    // Refresh button — friend mode only
    if (!m_friendIp.isEmpty()) {
        QPushButton *refreshBtn = makeBtn("Refresh", "#3a6a3a", "#2a5027");
        bottomRow->addWidget(refreshBtn);
        connect(refreshBtn, &QPushButton::clicked, this, [this, refreshBtn]() {
            if (m_actionTimer.elapsed() < 300) return;
            m_actionTimer.restart();
            refreshBtn->setEnabled(false);
            refreshBtn->setText("Loading...");
            const QByteArray data = fetchFromFriend("GET_LOG");
            populateList(data);
            refreshBtn->setText("Refresh");
            refreshBtn->setEnabled(true);
        });
    }

    QPushButton *closeBtn = makeBtn("Close", "#444444", "#333333");
    connect(closeBtn, &QPushButton::clicked, this, [this]() {
        if (m_actionTimer.elapsed() < 300) return;
        m_actionTimer.restart();
        accept();
    });
    bottomRow->addWidget(closeBtn);

    root->addWidget(m_listWidget, 1);
    root->addWidget(m_summaryLabel);
    root->addLayout(bottomRow);
}

// ── Populate list from raw log bytes ─────────────────────────────────────────
void StatDialog::populateList(const QByteArray &logData)
{
    m_listWidget->clear();
    const bool isFriend = !m_friendIp.isEmpty();

    int count = 0;
    QTextStream in(logData);
    while (!in.atEnd()) {
        const QString line = in.readLine().trimmed();
        if (line.isEmpty()) continue;

        ++count;
        QString alarmTime, dismissTime, photoPath;
        const QStringList parts = line.split(" | ", Qt::SkipEmptyParts);
        for (const QString &part : parts) {
            if      (part.startsWith("ALARM: "))     alarmTime   = part.mid(7).trimmed();
            else if (part.startsWith("DISMISSED: ")) dismissTime = part.mid(11).trimmed();
            else if (part.startsWith("PHOTO: "))     photoPath   = part.mid(7).trimmed();
        }
        if (alarmTime.isEmpty()) alarmTime = line;

        // View button visible:
        //   local  – photo path recorded AND file exists on this disk
        //   friend – photo path recorded (file lives on friend's board)
        const bool hasPhoto = !photoPath.isEmpty() &&
                              (isFriend || QFile::exists(photoPath));

        QWidget *rowWidget = new QWidget();
        rowWidget->setStyleSheet("background: transparent;");
        QHBoxLayout *rowLayout = new QHBoxLayout(rowWidget);
        rowLayout->setContentsMargins(4, 4, 4, 4);
        rowLayout->setSpacing(0);

        auto cell = [rowWidget](const QString &text, const QString &color = "#ffffff") {
            QLabel *l = new QLabel(text, rowWidget);
            l->setStyleSheet(
                QString("QLabel { font-size: 14px; color: %1; background: transparent; }").arg(color)
            );
            l->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
            return l;
        };

        QLabel *numLbl = cell(QString::number(count), "#888888");
        numLbl->setFixedWidth(32);
        QLabel *alarmLbl   = cell(alarmTime);
        QLabel *dismissLbl = cell(
            dismissTime.isEmpty() ? "(no record)" : dismissTime,
            dismissTime.isEmpty() ? "#666666"     : "#66cc66"
        );

        QPushButton *photoBtn = new QPushButton("View", rowWidget);
        photoBtn->setFixedSize(96, 30);
        photoBtn->setStyleSheet(
            "QPushButton { font-size: 13px; font-weight: 700; color: white;"
            "    background: #3a5f9a; border: none; border-radius: 7px; }"
            "QPushButton:pressed { background: #2a4977; }"
        );
        photoBtn->setVisible(hasPhoto);

        if (hasPhoto) {
            connect(photoBtn, &QPushButton::clicked, this, [this, photoPath, isFriend]() {
                if (m_actionTimer.elapsed() < 300) return;
                m_actionTimer.restart();

                QByteArray imgData;
                if (isFriend) {
                    imgData = fetchFromFriend("GET_PHOTO:" + photoPath);
                } else {
                    QFile f(photoPath);
                    if (f.open(QIODevice::ReadOnly))
                        imgData = f.readAll();
                }

                QDialog *dlg = new QDialog(this);
                dlg->setAttribute(Qt::WA_DeleteOnClose);
                dlg->setWindowTitle("Captured Photo");
                dlg->setFixedSize(760, 560);
                dlg->setStyleSheet(
                    "QDialog { background: #0b0b0b; border: 2px solid #ffffff; border-radius: 4px; }"
                );

                QVBoxLayout *vl = new QVBoxLayout(dlg);
                vl->setContentsMargins(12, 12, 12, 12);

                QLabel *img = new QLabel(dlg);
                img->setAlignment(Qt::AlignCenter);
                img->setStyleSheet("QLabel { background: #141414; border: 1px solid #3a3a3a; }");

                if (imgData.isEmpty()) {
                    img->setText("Failed to load photo");
                } else {
                    QImage image;
                    image.loadFromData(imgData);
                    if (image.isNull()) {
                        img->setText("Failed to decode photo");
                    } else {
                        img->setPixmap(QPixmap::fromImage(image).scaled(
                            720, 500, Qt::KeepAspectRatio, Qt::SmoothTransformation));
                    }
                }
                vl->addWidget(img, 1);

                QPushButton *closeBtn = new QPushButton("Close", dlg);
                closeBtn->setFixedHeight(38);
                closeBtn->setStyleSheet(
                    "QPushButton { font-size: 15px; font-weight: 700; color: white;"
                    "    background: #3f3f3f; border: none; border-radius: 8px; }"
                    "QPushButton:pressed { background: #2e2e2e; }"
                );
                connect(closeBtn, &QPushButton::clicked, dlg, &QDialog::accept);
                vl->addWidget(closeBtn);
                dlg->exec();
            });
        }

        QWidget *photoCell = new QWidget(rowWidget);
        photoCell->setFixedWidth(120);
        photoCell->setStyleSheet("background: transparent;");
        QHBoxLayout *photoCellLayout = new QHBoxLayout(photoCell);
        photoCellLayout->setContentsMargins(0, 0, 0, 0);
        photoCellLayout->setSpacing(0);
        photoCellLayout->addStretch();
        photoCellLayout->addWidget(photoBtn);
        photoCellLayout->addStretch();

        rowLayout->addWidget(numLbl);
        rowLayout->addWidget(alarmLbl,   5);
        rowLayout->addWidget(dismissLbl, 5);
        rowLayout->addWidget(photoCell);

        QListWidgetItem *item = new QListWidgetItem();
        item->setSizeHint(QSize(0, 52));
        m_listWidget->addItem(item);
        m_listWidget->setItemWidget(item, rowWidget);
    }

    if (count == 0) {
        const QString msg = isFriend
            ? "No records found (check connection or friend has no alarms yet)."
            : "No records found.";
        QListWidgetItem *empty = new QListWidgetItem(msg, m_listWidget);
        empty->setForeground(QColor("#666666"));
        empty->setTextAlignment(Qt::AlignCenter);
    }

    m_summaryLabel->setText(
        count > 0 ? QString("Total %1 record(s)").arg(count)
                  : "Log file is empty or not found."
    );
}
