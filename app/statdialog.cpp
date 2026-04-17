#include "statdialog.h"

#include <QFile>
#include <QHBoxLayout>
#include <QImageReader>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPixmap>
#include <QPushButton>
#include <QTextStream>
#include <QVBoxLayout>

StatDialog::StatDialog(const QString &logFilePath, QWidget *parent, const QString &title)
    : QDialog(parent)
{
    buildUi(logFilePath, title);
}

void StatDialog::buildUi(const QString &logFilePath, const QString &title)
{
    setWindowTitle(title.isEmpty() ? "Alarm Statistics" : title);
    setFixedSize(780, 500);
    setStyleSheet("background: #0b0b0b;");

    QVBoxLayout *root = new QVBoxLayout(this);
    root->setContentsMargins(20, 16, 20, 16);
    root->setSpacing(12);

    QLabel *titleLabel = new QLabel(title.isEmpty() ? "Alarm Dismiss History" : title, this);
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

    auto makeHeaderLabel = [headerWidget](const QString &text, int stretch) -> QLabel * {
        QLabel *lbl = new QLabel(text, headerWidget);
        lbl->setStyleSheet(
            "QLabel { font-size: 13px; font-weight: 700; color: #aaaaaa; background: transparent; }"
        );
        lbl->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        Q_UNUSED(stretch);
        return lbl;
    };

    QLabel *h1 = makeHeaderLabel("#",          1);
    QLabel *h2 = makeHeaderLabel("Alarm Time", 5);
    QLabel *h3 = makeHeaderLabel("Dismissed At", 5);
    QLabel *h4 = makeHeaderLabel("Photo", 2);
    h1->setFixedWidth(32);
    h4->setAlignment(Qt::AlignCenter);
    h4->setFixedWidth(120);

    headerLayout->addWidget(h1);
    headerLayout->addWidget(h2, 5);
    headerLayout->addWidget(h3, 5);
    headerLayout->addWidget(h4);
    root->addWidget(headerWidget);

    // List
    QListWidget *listWidget = new QListWidget(this);
    listWidget->setStyleSheet(
        "QListWidget {"
        "    background: #141414; border: 1px solid #3a3a3a;"
        "    border-radius: 8px; outline: none; padding: 4px;"
        "}"
        "QListWidget::item { padding: 2px 6px; border-radius: 4px; }"
        "QListWidget::item:alternate { background: #1a1a1a; }"
        "QListWidget::item:selected { background: #2d7dff; }"
    );
    listWidget->setAlternatingRowColors(true);
    listWidget->setSelectionMode(QAbstractItemView::NoSelection);

    // Parse log file
    // Each line format:
    //   ALARM: yyyy-MM-dd hh:mm | DISMISSED: yyyy-MM-dd hh:mm:ss | PHOTO: /path/file.jpg
    int count = 0;
    QFile file(logFilePath);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&file);
        while (!in.atEnd()) {
            const QString line = in.readLine().trimmed();
            if (line.isEmpty()) continue;

            ++count;
            QString alarmTime;
            QString dismissTime;
            QString photoPath;

            const QStringList parts = line.split(" | ", Qt::SkipEmptyParts);
            for (const QString &part : parts) {
                if (part.startsWith("ALARM: ")) {
                    alarmTime = part.mid(7).trimmed();
                } else if (part.startsWith("DISMISSED: ")) {
                    dismissTime = part.mid(11).trimmed();
                } else if (part.startsWith("PHOTO: ")) {
                    photoPath = part.mid(7).trimmed();
                }
            }
            if (alarmTime.isEmpty()) {
                alarmTime = line;
            }

            // Build a custom widget row
            QWidget *rowWidget = new QWidget();
            rowWidget->setStyleSheet("background: transparent;");
            QHBoxLayout *rowLayout = new QHBoxLayout(rowWidget);
            rowLayout->setContentsMargins(4, 4, 4, 4);
            rowLayout->setSpacing(0);

            auto cell = [rowWidget](const QString &text, const QString &color = "#ffffff") -> QLabel * {
                QLabel *l = new QLabel(text, rowWidget);
                l->setStyleSheet(
                    QString("QLabel { font-size: 14px; color: %1; background: transparent; }").arg(color)
                );
                l->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
                return l;
            };

            QLabel *numLbl     = cell(QString::number(count), "#888888");
            numLbl->setFixedWidth(32);
            QLabel *alarmLbl   = cell(alarmTime,   "#ffffff");
            QLabel *dismissLbl = cell(dismissTime.isEmpty() ? "(no record)" : dismissTime,
                                      dismissTime.isEmpty() ? "#666666" : "#66cc66");

            QPushButton *photoBtn = new QPushButton("View", rowWidget);
            photoBtn->setFixedSize(96, 30);
            photoBtn->setStyleSheet(
                "QPushButton { font-size: 13px; font-weight: 700; color: white;"
                "    background: #3a5f9a; border: none; border-radius: 7px; }"
                "QPushButton:pressed { background: #2a4977; }"
            );

            const bool hasPhoto = !photoPath.isEmpty() && QFile::exists(photoPath);
            photoBtn->setVisible(hasPhoto);
            if (hasPhoto) {
                connect(photoBtn, &QPushButton::clicked, this, [this, photoPath]() {
                    QDialog *dlg = new QDialog(this);
                    dlg->setAttribute(Qt::WA_DeleteOnClose);
                    dlg->setWindowTitle("Captured Photo");
                    dlg->setFixedSize(760, 560);
                    dlg->setStyleSheet("background: #0b0b0b;");

                    QVBoxLayout *vl = new QVBoxLayout(dlg);
                    vl->setContentsMargins(12, 12, 12, 12);

                    QLabel *img = new QLabel(dlg);
                    img->setAlignment(Qt::AlignCenter);
                    img->setStyleSheet("QLabel { background: #141414; border: 1px solid #3a3a3a; }");

                    QImageReader reader(photoPath);
                    const QImage image = reader.read();
                    if (image.isNull()) {
                        img->setText("Failed to load photo");
                    } else {
                        img->setPixmap(QPixmap::fromImage(image).scaled(
                            720, 500, Qt::KeepAspectRatio, Qt::SmoothTransformation));
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
            listWidget->addItem(item);
            listWidget->setItemWidget(item, rowWidget);
        }
        file.close();
    }

    if (count == 0) {
        QListWidgetItem *empty = new QListWidgetItem("No records found.", listWidget);
        empty->setForeground(QColor("#666666"));
        empty->setTextAlignment(Qt::AlignCenter);
    }

    // Summary label
    QLabel *summaryLabel = new QLabel(
        count > 0 ? QString("Total %1 record(s)").arg(count) : "Log file is empty or not found.",
        this
    );
    summaryLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    summaryLabel->setStyleSheet("QLabel { font-size: 12px; color: #888888; }");

    // Close button
    QPushButton *closeBtn = new QPushButton("Close", this);
    closeBtn->setFixedHeight(44);
    closeBtn->setStyleSheet(
        "QPushButton { font-size: 16px; font-weight: 700; color: white;"
        "    background: #444444; border: none; border-radius: 10px; }"
        "QPushButton:pressed { background: #333333; }"
    );
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);

    root->addWidget(listWidget, 1);
    root->addWidget(summaryLabel);
    root->addWidget(closeBtn);
}
