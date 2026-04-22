#include "statdialog.h"

#include <functional>
#include <QEventLoop>
#include <QFile>
#include <QFont>
#include <QDate>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QImage>
#include <QImageReader>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMap>
#include <QPainter>
#include <QPen>
#include <QPixmap>
#include <QPushButton>
#include <QStackedWidget>
#include <QTcpSocket>
#include <QTextStream>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

// ── CalendarWidget ────────────────────────────────────────────────────────────
// Calendar view: each day is colour-coded by how quickly the alarm was dismissed.
//   value 1 = green  (≤ 30 s)    value 2 = orange (≤ 2 min)    value 3 = red (> 2 min)
class CalendarWidget : public QWidget
{
public:
    explicit CalendarWidget(QWidget *parent = nullptr) : QWidget(parent)
    {
        m_month = QDate(QDate::currentDate().year(), QDate::currentDate().month(), 1);

        QVBoxLayout *root = new QVBoxLayout(this);
        root->setContentsMargins(16, 10, 16, 10);
        root->setSpacing(8);

        // ── Navigation row ────────────────────────────────────────────────────
        QHBoxLayout *nav = new QHBoxLayout();
        nav->setSpacing(8);

        const QString navStyle =
            "QPushButton { font-size:16px; font-weight:700; color:#cccccc;"
            "    background:#252525; border:none; border-radius:8px; }"
            "QPushButton:pressed { background:#3a3a3a; }";

        m_prevBtn = new QPushButton("<", this);
        m_prevBtn->setFixedSize(36, 32);
        m_prevBtn->setStyleSheet(navStyle);
        m_nextBtn = new QPushButton(">", this);
        m_nextBtn->setFixedSize(36, 32);
        m_nextBtn->setStyleSheet(navStyle);

        m_monthLabel = new QLabel(this);
        m_monthLabel->setAlignment(Qt::AlignCenter);
        m_monthLabel->setStyleSheet(
            "QLabel { font-size:15px; font-weight:700; color:#ffffff; background:transparent; }");

        nav->addWidget(m_prevBtn);
        nav->addWidget(m_monthLabel, 1);
        nav->addWidget(m_nextBtn);
        root->addLayout(nav);

        // ── Legend ────────────────────────────────────────────────────────────
        QHBoxLayout *leg = new QHBoxLayout();
        leg->setSpacing(16);
        leg->addStretch();
        const auto addLeg = [&](const QString &text, const QString &color) {
            QLabel *l = new QLabel(text, this);
            l->setStyleSheet(
                QString("QLabel { font-size:11px; color:%1; background:transparent; }").arg(color));
            leg->addWidget(l);
        };
        addLeg("\u25cf \u2264 30s (fast)", "#44cc44");
        addLeg("\u25cf \u2264 2min",        "#ff9933");
        addLeg("\u25cf > 2min (slow)",    "#ff5555");
        leg->addStretch();
        root->addLayout(leg);

        // ── Day-of-week header ────────────────────────────────────────────────
        m_gridLayout = new QGridLayout();
        m_gridLayout->setSpacing(4);
        m_gridLayout->setContentsMargins(0, 0, 0, 0);

        const QStringList dow = { "Sun","Mon","Tue","Wed","Thu","Fri","Sat" };
        for (int i = 0; i < 7; ++i) {
            QLabel *h = new QLabel(dow[i], this);
            h->setAlignment(Qt::AlignCenter);
            h->setFixedHeight(24);
            h->setStyleSheet(
                "QLabel { font-size:11px; font-weight:700; color:#666666; background:transparent; }");
            m_gridLayout->addWidget(h, 0, i);
        }
        root->addLayout(m_gridLayout, 1);

        connect(m_prevBtn, &QPushButton::clicked, this, [this]() {
            m_month = m_month.addMonths(-1);
            rebuild();
        });
        connect(m_nextBtn, &QPushButton::clicked, this, [this]() {
            m_month = m_month.addMonths(1);
            rebuild();
        });

        rebuild();
    }

    void setData(const QMap<QDate, int> &data)
    {
        m_data = data;
        if (!m_data.isEmpty()) {
            const QDate last = m_data.lastKey();
            m_month = QDate(last.year(), last.month(), 1);
        }
        rebuild();
    }

    std::function<void(QDate)> onDayClicked;

private:
    void rebuild()
    {
        m_monthLabel->setText(m_month.toString("MMMM yyyy"));

        // Delete old day-cell buttons
        for (QPushButton *l : m_dayLabels) {
            m_gridLayout->removeWidget(l);
            delete l;
        }
        m_dayLabels.clear();

        // Qt dayOfWeek(): 1=Mon..7=Sun → convert to 0=Sun..6=Sat
        const int firstDow    = m_month.dayOfWeek() % 7;
        const int daysInMonth = m_month.daysInMonth();

        for (int day = 1; day <= daysInMonth; ++day) {
            const int cellIdx = firstDow + day - 1;
            const int row     = cellIdx / 7 + 1;
            const int col     = cellIdx % 7;
            const QDate d     = QDate(m_month.year(), m_month.month(), day);
            const int   val   = m_data.value(d, 0);

            QString bg, fg, border;
            if      (val == 1) { bg = "#1a4a1a"; fg = "#44dd44"; border = "#2a7a2a"; }
            else if (val == 2) { bg = "#4a3010"; fg = "#ffaa33"; border = "#7a5020"; }
            else if (val == 3) { bg = "#4a1010"; fg = "#ff5555"; border = "#7a2020"; }
            else               { bg = "#1e1e1e"; fg = "#505050"; border = "#2a2a2a"; }

            QPushButton *btn = new QPushButton(QString::number(day), this);
            btn->setFixedHeight(42);
            btn->setFlat(true);
            btn->setStyleSheet(
                QString("QPushButton { font-size:14px; font-weight:%1; color:%2;"
                        "    background:%3; border:1px solid %4; border-radius:8px; }"
                        "QPushButton:hover { border-color:%5; }"
                        "QPushButton:pressed { background:%6; }")
                .arg(val > 0 ? "700" : "400").arg(fg).arg(bg).arg(border)
                .arg(val > 0 ? "#aaaaaa" : border)
                .arg(val > 0 ? fg : bg));
            if (val > 0) {
                btn->setCursor(Qt::PointingHandCursor);
                connect(btn, &QPushButton::clicked, this, [this, d]() {
                    if (onDayClicked) onDayClicked(d);
                });
            }
            m_gridLayout->addWidget(btn, row, col);
            m_dayLabels.append(btn);
        }
    }

    QGridLayout     *m_gridLayout = nullptr;
    QLabel          *m_monthLabel = nullptr;
    QPushButton     *m_prevBtn    = nullptr;
    QPushButton     *m_nextBtn    = nullptr;
    QDate            m_month;
    QMap<QDate, int> m_data;
    QList<QPushButton *> m_dayLabels;
};

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
            m_stack->setCurrentIndex(1);
            m_graphBtn->setText("List");
            refreshBtn->setText("Refresh");
            refreshBtn->setEnabled(true);
        });
    }

    m_graphBtn = makeBtn("List", "#4a3a7a", "#3a2a5a");
    QPushButton *closeBtn = makeBtn("Close", "#444444", "#333333");
    connect(closeBtn, &QPushButton::clicked, this, [this]() {
        if (m_actionTimer.elapsed() < 300) return;
        m_actionTimer.restart();
        accept();
    });
    bottomRow->addWidget(m_graphBtn);
    bottomRow->addWidget(closeBtn);

    // Stack page 0: header + list; page 1: calendar
    m_listDateLabel = new QLabel("All Records", this);
    m_listDateLabel->setAlignment(Qt::AlignCenter);
    m_listDateLabel->setStyleSheet(
        "QLabel { font-size: 13px; color: #aaaaaa; background: transparent; }");

    QWidget *listPage = new QWidget(this);
    QVBoxLayout *listPageLay = new QVBoxLayout(listPage);
    listPageLay->setContentsMargins(0, 0, 0, 0);
    listPageLay->setSpacing(4);
    listPageLay->addWidget(m_listDateLabel);
    listPageLay->addWidget(headerWidget);
    listPageLay->addWidget(m_listWidget, 1);

    m_chartWidget = new CalendarWidget(this);
    m_chartWidget->onDayClicked = [this](QDate d) {
        if (m_actionTimer.elapsed() < 300) return;
        m_actionTimer.restart();
        populateList(m_logData, d);
        m_stack->setCurrentIndex(0);
        m_graphBtn->setText("Calendar");
    };

    m_stack = new QStackedWidget(this);
    m_stack->addWidget(listPage);      // index 0
    m_stack->addWidget(m_chartWidget); // index 1
    m_stack->setCurrentIndex(1);       // start on calendar

    connect(m_graphBtn, &QPushButton::clicked, this, [this]() {
        if (m_actionTimer.elapsed() < 300) return;
        m_actionTimer.restart();
        if (m_stack->currentIndex() == 1) {
            // Calendar → show all records list
            populateList(m_logData);
            m_stack->setCurrentIndex(0);
            m_graphBtn->setText("Calendar");
        } else {
            // List → back to calendar
            m_stack->setCurrentIndex(1);
            m_graphBtn->setText("List");
        }
    });

    root->addWidget(m_stack, 1);
    root->addWidget(m_summaryLabel);
    root->addLayout(bottomRow);
}

// ── Populate list from raw log bytes ─────────────────────────────────────────
void StatDialog::populateList(const QByteArray &logData, QDate filterDate)
{
    m_logData = logData;
    if (m_listDateLabel)
        m_listDateLabel->setText(filterDate.isValid()
            ? QString("Records for %1").arg(filterDate.toString("MMMM d, yyyy"))
            : "All Records");
    m_listWidget->clear();
    const bool isFriend = !m_friendIp.isEmpty();

    int count = 0;
    QMap<QDate, int> calData;
    QTextStream in(logData);
    while (!in.atEnd()) {
        const QString line = in.readLine().trimmed();
        if (line.isEmpty()) continue;

        QString alarmTime, dismissTime, photoPath;
        const QStringList parts = line.split(" | ", Qt::SkipEmptyParts);
        for (const QString &part : parts) {
            if      (part.startsWith("ALARM: "))     alarmTime   = part.mid(7).trimmed();
            else if (part.startsWith("DISMISSED: ")) dismissTime = part.mid(11).trimmed();
            else if (part.startsWith("PHOTO: "))     photoPath   = part.mid(7).trimmed();
        }
        if (alarmTime.isEmpty()) alarmTime = line;

        // Parse datetimes
        QDateTime alarmDt, dismissDt;
        if (!alarmTime.isEmpty() && !dismissTime.isEmpty()) {
            alarmDt   = QDateTime::fromString(alarmTime,   "yyyy-MM-dd hh:mm");
            dismissDt = QDateTime::fromString(dismissTime, "yyyy-MM-dd hh:mm:ss");
        }

        // When filtering by date: skip records not on that day
        if (filterDate.isValid()) {
            if (!dismissDt.isValid() || dismissDt.date() != filterDate)
                continue;
        }

        ++count;

        // Colour-code for calendar (only when not filtering)
        if (!filterDate.isValid() && alarmDt.isValid() && dismissDt.isValid()) {
            const int secs = static_cast<int>(alarmDt.secsTo(dismissDt));
            int colorVal;
            if      (secs <= 30)  colorVal = 1;
            else if (secs <= 120) colorVal = 2;
            else                  colorVal = 3;
            const QDate date = dismissDt.date();
            calData[date] = qMax(calData.value(date, 0), colorVal);
        }

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
        const QString msg = filterDate.isValid()
            ? "No records found for this date."
            : (isFriend ? "No records found (check connection or friend has no alarms yet)."
                        : "No records found.");
        QListWidgetItem *empty = new QListWidgetItem(msg, m_listWidget);
        empty->setForeground(QColor("#666666"));
        empty->setTextAlignment(Qt::AlignCenter);
    }

    m_summaryLabel->setText(
        count > 0 ? QString("Total %1 record(s)").arg(count)
                  : (filterDate.isValid() ? "No records for this date."
                                          : "Log file is empty or not found.")
    );

    // Update calendar only when showing all records
    if (!filterDate.isValid())
        m_chartWidget->setData(calData);
}
