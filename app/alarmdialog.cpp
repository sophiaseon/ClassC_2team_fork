#include "alarmdialog.h"

#include <QComboBox>
#include <QDialog>
#include <QDateTime>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QStackedWidget>
#include <QTime>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

namespace {

int weekdayIndexFromDate(const QDate &date)
{
    return date.dayOfWeek() % 7; // Mon=1..Sun=7 -> Sun=0..Sat=6
}

int daysInMonth(int year, int month)
{
    return QDate(year, month, 1).daysInMonth();
}

QLabel *createFieldNameLabel(const QString &text, QWidget *parent)
{
    QLabel *label = new QLabel(text, parent);
    label->setAlignment(Qt::AlignCenter);
    label->setStyleSheet(
        "QLabel {"
        "    font-size: 15px;"
        "    font-weight: 600;"
        "    color: #dddddd;"
        "}"
    );
    return label;
}

QLabel *createValueLabel(QWidget *parent)
{
    QLabel *label = new QLabel(parent);
    label->setAlignment(Qt::AlignCenter);
    label->setFixedSize(170, 76);
    label->setStyleSheet(
        "QLabel {"
        "    font-size: 32px;"
        "    font-weight: 700;"
        "    color: white;"
        "    background: #202020;"
        "    border: 1px solid #4a4a4a;"
        "    border-radius: 12px;"
        "}"
    );
    return label;
}

QPushButton *createAdjustButton(const QString &text, QWidget *parent)
{
    QPushButton *button = new QPushButton(text, parent);
    button->setFixedSize(90, 80);
    button->setStyleSheet(
        "QPushButton {"
        "    font-size: 32px;"
        "    font-weight: 700;"
        "    color: white;"
        "    background: #2c2c2c;"
        "    border: 1px solid #4a4a4a;"
        "    border-radius: 14px;"
        "}"
        "QPushButton:pressed {"
        "    background: #4a4a4a;"
        "}"
    );
    return button;
}

} // namespace

AlarmDialog::AlarmDialog(QWidget *parent,
                         int editIndex,
                         const QDateTime &initialDt,
                         const QString &initialSound,
                         int initialDismissMode,
                         int initialGameType,
                         int initialRepeatMask,
                         bool initialUseSpecificDate)
    : QDialog(parent)
    , m_editIndex(editIndex)
    , m_soundResult(initialSound.isEmpty() ? "/mnt/nfs/test_contents/test.wav" : initialSound)
    , m_dismissMode(initialDismissMode)
    , m_gameType(initialGameType)
    , m_repeatMask(initialRepeatMask)
    , m_useSpecificDate(initialUseSpecificDate)
    , m_specificDate(initialDt.isValid() ? initialDt.date() : QDate::currentDate())
    , m_ampm(0)
    , m_hour12(12)
    , m_minute(0)
    , m_ampmValueLabel(nullptr)
    , m_hourValueLabel(nullptr)
    , m_minuteValueLabel(nullptr)
    , m_ampmPlusButton(nullptr)
    , m_ampmMinusButton(nullptr)
    , m_hourPlusButton(nullptr)
    , m_hourMinusButton(nullptr)
    , m_minutePlusButton(nullptr)
    , m_minuteMinusButton(nullptr)
    , m_dateSummaryLabel(nullptr)
    , m_calendarToggleBtn(nullptr)
    , m_soundCombo(nullptr)
    , m_gameCombo(nullptr)
    , m_gameOptionStack(nullptr)
    , m_simpleModeBtn(nullptr)
    , m_gameModeBtn(nullptr)
    , m_buttonModeBtn(nullptr)
    , m_cameraModeBtn(nullptr)
    , m_ultrasonicModeBtn(nullptr)
{
    for (int i = 0; i < 7; ++i) m_weekdayButtons[i] = nullptr;

    buildUi();
    m_adjustTimer.start();

    const QDateTime base = initialDt.isValid() ? initialDt : QDateTime::currentDateTime();
    setTimeFromDateTime(base);
    setRepeatButtonsFromMask(m_repeatMask);

    const int soundIdx = m_soundCombo->findData(m_soundResult);
    if (soundIdx >= 0) m_soundCombo->setCurrentIndex(soundIdx);

    const int gameIdx = m_gameCombo->findData(m_gameType);
    if (gameIdx >= 0) m_gameCombo->setCurrentIndex(gameIdx);

    refreshTimeLabels();
    refreshModeStyle();
    refreshDateSummary();
}

QDateTime AlarmDialog::selectedDateTime() const
{
    return m_result;
}

QString AlarmDialog::soundFile() const
{
    return m_soundResult;
}

int AlarmDialog::dismissMode() const
{
    return m_dismissMode;
}

int AlarmDialog::gameType() const
{
    return m_gameType;
}

int AlarmDialog::repeatMask() const
{
    return m_repeatMask;
}

bool AlarmDialog::useSpecificDate() const
{
    return m_useSpecificDate;
}

void AlarmDialog::buildUi()
{
    setWindowTitle(m_editIndex < 0 ? "Add Alarm" : "Edit Alarm");
    setFixedSize(900, 550);
    setStyleSheet("background: #0b0b0b;");

    QVBoxLayout *root = new QVBoxLayout(this);
    root->setContentsMargins(20, 8, 20, 8);
    root->setSpacing(6);

    QLabel *titleLabel = new QLabel(m_editIndex < 0 ? "Add Alarm" : "Edit Alarm", this);
    titleLabel->setAlignment(Qt::AlignCenter);
    titleLabel->setStyleSheet("QLabel { font-size: 20px; font-weight: 700; color: white; }");
    root->addWidget(titleLabel);

    QGridLayout *timeGrid = new QGridLayout();
    timeGrid->setHorizontalSpacing(24);
    timeGrid->setVerticalSpacing(4);

    timeGrid->addWidget(createFieldNameLabel("AM/PM", this), 0, 0, Qt::AlignCenter);
    timeGrid->addWidget(createFieldNameLabel("Hour", this), 0, 1, Qt::AlignCenter);
    timeGrid->addWidget(createFieldNameLabel("Minute", this), 0, 2, Qt::AlignCenter);

    auto buildTimeField = [this, &timeGrid](int col,
                                            QPushButton *&plus,
                                            QLabel *&val,
                                            QPushButton *&minus) {
        QWidget *w = new QWidget(this);
        QVBoxLayout *vl = new QVBoxLayout(w);
        vl->setContentsMargins(0, 0, 0, 0);
        vl->setSpacing(4);
        plus  = createAdjustButton("+", w);
        val   = createValueLabel(w);
        minus = createAdjustButton("-", w);
        vl->addWidget(plus,  0, Qt::AlignCenter);
        vl->addWidget(val,   0, Qt::AlignCenter);
        vl->addWidget(minus, 0, Qt::AlignCenter);
        timeGrid->addWidget(w, 1, col, Qt::AlignCenter);
    };

    buildTimeField(0, m_ampmPlusButton, m_ampmValueLabel, m_ampmMinusButton);
    buildTimeField(1, m_hourPlusButton, m_hourValueLabel, m_hourMinusButton);
    buildTimeField(2, m_minutePlusButton, m_minuteValueLabel, m_minuteMinusButton);

    QWidget *calendarBtnWrap = new QWidget(this);
    QVBoxLayout *calendarBtnLayout = new QVBoxLayout(calendarBtnWrap);
    calendarBtnLayout->setContentsMargins(0, 10, 0, 0); // Move weekday and below sections downward
    calendarBtnLayout->setSpacing(6);

    m_calendarToggleBtn = new QPushButton("Calendar", this);
    m_calendarToggleBtn->setFixedHeight(50);
    m_calendarToggleBtn->setStyleSheet(
        "QPushButton { font-size: 16px; font-weight: 700; color: white;"
        "    background: #444444; border: none; border-radius: 10px; padding: 0 16px; }"
        "QPushButton:pressed { background: #333333; }"
    );

    m_dateSummaryLabel = new QLabel(this);
    m_dateSummaryLabel->setStyleSheet("QLabel { font-size: 14px; color: #9e9e9e; }");
    m_dateSummaryLabel->setAlignment(Qt::AlignCenter);

    calendarBtnLayout->addStretch();
    calendarBtnLayout->addWidget(m_calendarToggleBtn);
    calendarBtnLayout->addWidget(m_dateSummaryLabel);
    calendarBtnLayout->addStretch();

    timeGrid->addWidget(calendarBtnWrap, 1, 3, Qt::AlignCenter);

    root->addLayout(timeGrid);

    QWidget *repeatPanel = new QWidget(this);
    QHBoxLayout *repeatLayout = new QHBoxLayout(repeatPanel);
    repeatLayout->setContentsMargins(0, 2, 0, 0);
    repeatLayout->setSpacing(8);

    const QString dayNames[7] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
    for (int i = 0; i < 7; ++i) {
        QPushButton *btn = new QPushButton(dayNames[i], this);
        btn->setCheckable(true);
        btn->setFixedHeight(40);
        btn->setStyleSheet(
            "QPushButton { font-size: 14px; font-weight: 700; color: #9e9e9e;"
            "    background: #242424; border: 1px solid #4a4a4a; border-radius: 9px; }"
            "QPushButton:checked { color: white; background: #2d7dff; border: none; }"
        );
        m_weekdayButtons[i] = btn;
        repeatLayout->addWidget(btn, 1);
    }
    root->addWidget(repeatPanel);

    QHBoxLayout *optionsRow = new QHBoxLayout();
    optionsRow->setSpacing(24);
    optionsRow->setContentsMargins(0, 0, 0, 0);

    auto makeCombo = [this](int h = 44) {
        QComboBox *cb = new QComboBox(this);
        cb->setFixedHeight(h);
        cb->setStyleSheet(
            "QComboBox {"
            "    font-size: 15px; color: white; background: #202020;"
            "    border: 1px solid #4a4a4a; border-radius: 8px; padding: 3px 10px;"
            "}"
            "QComboBox::drop-down { border: none; width: 24px; }"
            "QComboBox QAbstractItemView {"
            "    background: #202020; color: white;"
            "    selection-background-color: #2d7dff;"
            "}"
        );
        return cb;
    };

    QVBoxLayout *soundLayout = new QVBoxLayout();
    soundLayout->setSpacing(6);
    QLabel *soundLabel = new QLabel("Alarm Sound", this);
    soundLabel->setStyleSheet("QLabel { font-size: 14px; font-weight: 600; color: #aaaaaa; }");
    m_soundCombo = makeCombo();
    m_soundCombo->addItem("test.wav",        "/mnt/nfs/test_contents/test.wav");
    m_soundCombo->addItem("test2.wav",       "/mnt/nfs/test_contents/test2.wav");
    m_soundCombo->addItem("Tetris (Buzzer)", "buzzer:tetris");
    soundLayout->addWidget(soundLabel);
    soundLayout->addWidget(m_soundCombo);

    QVBoxLayout *modeLayout = new QVBoxLayout();
    modeLayout->setSpacing(6);
    QLabel *modeLabel = new QLabel("Dismiss Mode", this);
    modeLabel->setStyleSheet("QLabel { font-size: 14px; font-weight: 600; color: #aaaaaa; }");
    QHBoxLayout *modeBtnRow = new QHBoxLayout();
    modeBtnRow->setSpacing(10);

    m_simpleModeBtn = new QPushButton("Simple", this);
    m_gameModeBtn = new QPushButton("Game", this);
    m_buttonModeBtn = new QPushButton("Button", this);
    m_cameraModeBtn = new QPushButton("Camera", this);
    m_ultrasonicModeBtn = new QPushButton("Ultrasonic", this);

    QList<QPushButton *> modeButtons;
    modeButtons << m_simpleModeBtn << m_gameModeBtn << m_buttonModeBtn
                << m_cameraModeBtn << m_ultrasonicModeBtn;
    for (QPushButton *btn : modeButtons) {
        btn->setFixedHeight(38);
        modeBtnRow->addWidget(btn, 1);
    }

    m_gameCombo = makeCombo(40);
    m_gameCombo->addItem("Number Order (1-25)", GameNumberOrder);
    m_gameCombo->addItem("Color Memory (5-6-7)", GameColorMemory);

    m_gameOptionStack = new QStackedWidget(this);
    m_gameOptionStack->setFixedHeight(40);
    QWidget *blankPage = new QWidget(this);
    blankPage->setStyleSheet("background: transparent;");
    m_gameOptionStack->addWidget(blankPage);
    m_gameOptionStack->addWidget(m_gameCombo);

    modeLayout->addWidget(modeLabel);
    modeLayout->addLayout(modeBtnRow);
    modeLayout->addWidget(m_gameOptionStack);

    optionsRow->addLayout(soundLayout, 1);
    optionsRow->addLayout(modeLayout, 2);
    optionsRow->setAlignment(soundLayout, Qt::AlignTop);
    optionsRow->setAlignment(modeLayout, Qt::AlignTop);
    root->addLayout(optionsRow);

    root->addStretch();

    QHBoxLayout *bottomBtns = new QHBoxLayout();
    QPushButton *confirmBtn = new QPushButton(m_editIndex < 0 ? "Add Alarm" : "Save", this);
    QPushButton *cancelBtn = new QPushButton("Cancel", this);
    confirmBtn->setFixedHeight(42);
    cancelBtn->setFixedHeight(42);
    confirmBtn->setStyleSheet(
        "QPushButton { font-size: 17px; font-weight: 700; color: white;"
        "    background: #2d7dff; border: none; border-radius: 10px; }"
        "QPushButton:pressed { background: #1d5fc7; }"
    );
    cancelBtn->setStyleSheet(
        "QPushButton { font-size: 17px; font-weight: 700; color: white;"
        "    background: #444444; border: none; border-radius: 10px; }"
        "QPushButton:pressed { background: #333333; }"
    );
    bottomBtns->addWidget(confirmBtn, 1);
    bottomBtns->addWidget(cancelBtn, 1);
    root->addLayout(bottomBtns);

    connect(confirmBtn, &QPushButton::clicked, this, &AlarmDialog::onConfirm);
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);

    connect(m_ampmPlusButton, &QPushButton::clicked, this, &AlarmDialog::increaseAmPm);
    connect(m_ampmMinusButton, &QPushButton::clicked, this, &AlarmDialog::decreaseAmPm);
    connect(m_hourPlusButton, &QPushButton::clicked, this, &AlarmDialog::increaseHour);
    connect(m_hourMinusButton, &QPushButton::clicked, this, &AlarmDialog::decreaseHour);
    connect(m_minutePlusButton, &QPushButton::clicked, this, &AlarmDialog::increaseMinute);
    connect(m_minuteMinusButton, &QPushButton::clicked, this, &AlarmDialog::decreaseMinute);

    connect(m_simpleModeBtn, &QPushButton::clicked, this, [this]() {
        m_simpleModeBtn->setEnabled(false);
        QTimer::singleShot(500, this, [this]() { m_simpleModeBtn->setEnabled(true); });
        m_dismissMode = DismissSimple;
        refreshModeStyle();
    });
    connect(m_gameModeBtn, &QPushButton::clicked, this, [this]() {
        m_gameModeBtn->setEnabled(false);
        QTimer::singleShot(500, this, [this]() { m_gameModeBtn->setEnabled(true); });
        m_dismissMode = DismissGame;
        refreshModeStyle();
    });
    connect(m_buttonModeBtn, &QPushButton::clicked, this, [this]() {
        m_buttonModeBtn->setEnabled(false);
        QTimer::singleShot(500, this, [this]() { m_buttonModeBtn->setEnabled(true); });
        m_dismissMode = DismissButton;
        refreshModeStyle();
    });
    connect(m_cameraModeBtn, &QPushButton::clicked, this, [this]() {
        m_cameraModeBtn->setEnabled(false);
        QTimer::singleShot(500, this, [this]() { m_cameraModeBtn->setEnabled(true); });
        m_dismissMode = DismissCamera;
        refreshModeStyle();
    });
    connect(m_ultrasonicModeBtn, &QPushButton::clicked, this, [this]() {
        m_ultrasonicModeBtn->setEnabled(false);
        QTimer::singleShot(500, this, [this]() { m_ultrasonicModeBtn->setEnabled(true); });
        m_dismissMode = DismissUltrasonic;
        refreshModeStyle();
    });

    // m_gameCombo: activated fires only on user popup selection (not programmatic setCurrentIndex)
    connect(m_gameCombo, QOverload<int>::of(&QComboBox::activated), this, [this](int idx) {
        m_gameType = m_gameCombo->itemData(idx).toInt();
        m_gameCombo->setEnabled(false);
        QTimer::singleShot(300, m_gameCombo, [this]() { if (m_gameCombo) m_gameCombo->setEnabled(true); });
    });

    // m_soundCombo: disable for 300ms after selection to block phantom re-opens
    connect(m_soundCombo, QOverload<int>::of(&QComboBox::activated), this, [this](int) {
        m_soundCombo->setEnabled(false);
        QTimer::singleShot(300, m_soundCombo, [this]() { if (m_soundCombo) m_soundCombo->setEnabled(true); });
    });

    connect(m_calendarToggleBtn, &QPushButton::clicked, this, [this]() {
        m_calendarToggleBtn->setEnabled(false);
        QTimer::singleShot(500, this, [this]() { m_calendarToggleBtn->setEnabled(true); });
        openCalendarDialog();
    });

    // Per-button debounce lock: each weekday button has its own lock, so phantom clicks
    // on that button are reverted while other buttons remain fully responsive.
    for (int i = 0; i < 7; ++i) {
        m_weekdayLocked[i] = false;
        connect(m_weekdayButtons[i], &QPushButton::clicked, this, [this, i](bool checked) {
            if (m_weekdayLocked[i]) {
                // Revert the toggle Qt already applied
                m_weekdayButtons[i]->blockSignals(true);
                m_weekdayButtons[i]->setChecked(!checked);
                m_weekdayButtons[i]->blockSignals(false);
                return;
            }
            m_weekdayLocked[i] = true;
            QTimer::singleShot(300, this, [this, i]() { m_weekdayLocked[i] = false; });
            refreshDateSummary();
        });
    }
}

void AlarmDialog::refreshModeStyle()
{
    const QString on =
        "QPushButton { font-size: 15px; font-weight: 700; color: white;"
        "    background: #2d7dff; border: none; border-radius: 8px; }";
    const QString off =
        "QPushButton { font-size: 15px; font-weight: 700; color: #888888;"
        "    background: #2c2c2c; border: 1px solid #4a4a4a; border-radius: 8px; }";

    m_simpleModeBtn->setStyleSheet(m_dismissMode == DismissSimple ? on : off);
    m_gameModeBtn->setStyleSheet(m_dismissMode == DismissGame ? on : off);
    m_buttonModeBtn->setStyleSheet(m_dismissMode == DismissButton ? on : off);
    m_cameraModeBtn->setStyleSheet(m_dismissMode == DismissCamera ? on : off);
    m_ultrasonicModeBtn->setStyleSheet(m_dismissMode == DismissUltrasonic ? on : off);
    m_gameOptionStack->setCurrentIndex(m_dismissMode == DismissGame ? 1 : 0);
}

void AlarmDialog::setTimeFromDateTime(const QDateTime &dt)
{
    int h = dt.time().hour();
    m_ampm = (h >= 12) ? 1 : 0;

    if (h == 0) h = 12;
    else if (h > 12) h -= 12;

    m_hour12 = h;
    m_minute = dt.time().minute();
}

QTime AlarmDialog::selectedTime() const
{
    int h = m_hour12;
    if (h == 12) h = (m_ampm == 1) ? 12 : 0;
    else if (m_ampm == 1) h += 12;

    return QTime(h, m_minute, 0);
}

void AlarmDialog::refreshTimeLabels()
{
    m_ampmValueLabel->setText(m_ampm == 0 ? "AM" : "PM");
    m_hourValueLabel->setText(QString::number(m_hour12));
    m_minuteValueLabel->setText(QString("%1").arg(m_minute, 2, 10, QLatin1Char('0')));
}

void AlarmDialog::increaseAmPm()
{
    m_ampmPlusButton->setEnabled(false);
    QTimer::singleShot(400, this, [this]() { m_ampmPlusButton->setEnabled(true); });
    m_ampm = 1 - m_ampm;
    refreshTimeLabels();
}

void AlarmDialog::decreaseAmPm()
{
    m_ampmMinusButton->setEnabled(false);
    QTimer::singleShot(400, this, [this]() { m_ampmMinusButton->setEnabled(true); });
    m_ampm = 1 - m_ampm;
    refreshTimeLabels();
}

void AlarmDialog::increaseHour()
{
    m_hourPlusButton->setEnabled(false);
    QTimer::singleShot(400, this, [this]() { m_hourPlusButton->setEnabled(true); });
    ++m_hour12;
    if (m_hour12 > 12) m_hour12 = 1;
    refreshTimeLabels();
}

void AlarmDialog::decreaseHour()
{
    m_hourMinusButton->setEnabled(false);
    QTimer::singleShot(400, this, [this]() { m_hourMinusButton->setEnabled(true); });
    --m_hour12;
    if (m_hour12 < 1) m_hour12 = 12;
    refreshTimeLabels();
}

void AlarmDialog::increaseMinute()
{
    m_minutePlusButton->setEnabled(false);
    QTimer::singleShot(400, this, [this]() { m_minutePlusButton->setEnabled(true); });
    ++m_minute;
    if (m_minute > 59) m_minute = 0;
    refreshTimeLabels();
}

void AlarmDialog::decreaseMinute()
{
    m_minuteMinusButton->setEnabled(false);
    QTimer::singleShot(400, this, [this]() { m_minuteMinusButton->setEnabled(true); });
    --m_minute;
    if (m_minute < 0) m_minute = 59;
    refreshTimeLabels();
}

void AlarmDialog::setRepeatButtonsFromMask(int mask)
{
    for (int i = 0; i < 7; ++i) {
        m_weekdayButtons[i]->setChecked((mask & (1 << i)) != 0);
    }
}

int AlarmDialog::repeatMaskFromButtons() const
{
    int mask = 0;
    for (int i = 0; i < 7; ++i) {
        if (m_weekdayButtons[i]->isChecked()) mask |= (1 << i);
    }
    return mask;
}

QDateTime AlarmDialog::computeNextForRepeat(const QDateTime &now, const QTime &time, int mask) const
{
    for (int offset = 0; offset <= 7; ++offset) {
        const QDate candidateDate = now.date().addDays(offset);
        const int idx = weekdayIndexFromDate(candidateDate);
        if ((mask & (1 << idx)) == 0) continue;

        const QDateTime candidate(candidateDate, time);
        if (candidate > now) return candidate;
    }

    return QDateTime(now.date().addDays(1), time);
}

void AlarmDialog::refreshDateSummary()
{
    const int mask = repeatMaskFromButtons();
    if (mask != 0) {
        m_dateSummaryLabel->setText("Weekly repeat enabled");
        return;
    }

    if (m_useSpecificDate) {
        m_dateSummaryLabel->setText(
            QString("Date: %1").arg(m_specificDate.toString("yyyy-MM-dd"))
        );
        return;
    }

    // Requested: hide auto-date explanation text.
    m_dateSummaryLabel->clear();
}

void AlarmDialog::onConfirm()
{
    m_soundResult = m_soundCombo->currentData().toString();
    m_repeatMask = repeatMaskFromButtons();

    if (m_repeatMask != 0) {
        m_useSpecificDate = false;
    }

    const QDateTime now = QDateTime::currentDateTime();
    const QTime t = selectedTime();

    if (m_repeatMask != 0) {
        m_result = computeNextForRepeat(now, t, m_repeatMask);
    } else if (m_useSpecificDate) {
        m_result = QDateTime(m_specificDate, t);
    } else {
        QDate d = now.date();
        if (t <= now.time()) d = d.addDays(1);
        m_result = QDateTime(d, t);
    }

    accept();
}

void AlarmDialog::openCalendarDialog()
{
    QDialog dlg(this);
    dlg.setWindowTitle("Select Date");
    dlg.setFixedSize(620, 420);
    dlg.setStyleSheet("background: #0b0b0b;");

    int year = m_specificDate.year();
    int month = m_specificDate.month();
    int day = m_specificDate.day();

    QVBoxLayout *root = new QVBoxLayout(&dlg);
    root->setContentsMargins(18, 14, 18, 14);
    root->setSpacing(10);

    QLabel *title = new QLabel("Date Selector", &dlg);
    title->setAlignment(Qt::AlignCenter);
    title->setStyleSheet("QLabel { font-size: 18px; font-weight: 700; color: white; }");
    root->addWidget(title);

    QGridLayout *grid = new QGridLayout();
    grid->setHorizontalSpacing(16);
    grid->setVerticalSpacing(8);

    QLabel *yearLblTitle = createFieldNameLabel("Year", &dlg);
    QLabel *monthLblTitle = createFieldNameLabel("Month", &dlg);
    QLabel *dayLblTitle = createFieldNameLabel("Day", &dlg);
    grid->addWidget(yearLblTitle, 0, 0, Qt::AlignCenter);
    grid->addWidget(monthLblTitle, 0, 1, Qt::AlignCenter);
    grid->addWidget(dayLblTitle, 0, 2, Qt::AlignCenter);

    QPushButton *yearPlus = createAdjustButton("+", &dlg);
    QPushButton *yearMinus = createAdjustButton("-", &dlg);
    QPushButton *monthPlus = createAdjustButton("+", &dlg);
    QPushButton *monthMinus = createAdjustButton("-", &dlg);
    QPushButton *dayPlus = createAdjustButton("+", &dlg);
    QPushButton *dayMinus = createAdjustButton("-", &dlg);

    QLabel *yearValue = createValueLabel(&dlg);
    QLabel *monthValue = createValueLabel(&dlg);
    QLabel *dayValue = createValueLabel(&dlg);

    auto refresh = [&]() {
        const int maxDay = daysInMonth(year, month);
        if (day > maxDay) day = maxDay;
        if (day < 1) day = 1;

        yearValue->setText(QString::number(year));
        monthValue->setText(QString::number(month));
        dayValue->setText(QString::number(day));
    };

    QElapsedTimer calTimer;
    calTimer.start();

    connect(yearPlus, &QPushButton::clicked, &dlg, [&]() {
        if (calTimer.elapsed() < 300) return; calTimer.restart();
        ++year; if (year > 2099) year = 2000; refresh();
    });
    connect(yearMinus, &QPushButton::clicked, &dlg, [&]() {
        if (calTimer.elapsed() < 300) return; calTimer.restart();
        --year; if (year < 2000) year = 2099; refresh();
    });
    connect(monthPlus, &QPushButton::clicked, &dlg, [&]() {
        if (calTimer.elapsed() < 300) return; calTimer.restart();
        ++month; if (month > 12) month = 1; refresh();
    });
    connect(monthMinus, &QPushButton::clicked, &dlg, [&]() {
        if (calTimer.elapsed() < 300) return; calTimer.restart();
        --month; if (month < 1) month = 12; refresh();
    });
    connect(dayPlus, &QPushButton::clicked, &dlg, [&]() {
        if (calTimer.elapsed() < 300) return; calTimer.restart();
        ++day;
        int maxDay = daysInMonth(year, month);
        if (day > maxDay) day = 1;
        refresh();
    });
    connect(dayMinus, &QPushButton::clicked, &dlg, [&]() {
        if (calTimer.elapsed() < 300) return; calTimer.restart();
        --day;
        int maxDay = daysInMonth(year, month);
        if (day < 1) day = maxDay;
        refresh();
    });

    grid->addWidget(yearPlus, 1, 0, Qt::AlignCenter);
    grid->addWidget(monthPlus, 1, 1, Qt::AlignCenter);
    grid->addWidget(dayPlus, 1, 2, Qt::AlignCenter);
    grid->addWidget(yearValue, 2, 0, Qt::AlignCenter);
    grid->addWidget(monthValue, 2, 1, Qt::AlignCenter);
    grid->addWidget(dayValue, 2, 2, Qt::AlignCenter);
    grid->addWidget(yearMinus, 3, 0, Qt::AlignCenter);
    grid->addWidget(monthMinus, 3, 1, Qt::AlignCenter);
    grid->addWidget(dayMinus, 3, 2, Qt::AlignCenter);

    root->addLayout(grid);
    root->addStretch();

    QHBoxLayout *btns = new QHBoxLayout();
    QPushButton *setBtn = new QPushButton("Set Date", &dlg);
    QPushButton *autoBtn = new QPushButton("Use Auto Date", &dlg);
    QPushButton *cancelBtn = new QPushButton("Cancel", &dlg);
    setBtn->setFixedHeight(42);
    autoBtn->setFixedHeight(42);
    cancelBtn->setFixedHeight(42);
    setBtn->setStyleSheet(
        "QPushButton { font-size: 14px; font-weight: 700; color: white;"
        "    background: #2d7dff; border: none; border-radius: 8px; }"
        "QPushButton:pressed { background: #1d5fc7; }"
    );
    autoBtn->setStyleSheet(
        "QPushButton { font-size: 14px; font-weight: 700; color: white;"
        "    background: #555555; border: none; border-radius: 8px; }"
        "QPushButton:pressed { background: #3d3d3d; }"
    );
    cancelBtn->setStyleSheet(
        "QPushButton { font-size: 14px; font-weight: 700; color: white;"
        "    background: #333333; border: none; border-radius: 8px; }"
        "QPushButton:pressed { background: #252525; }"
    );

    connect(setBtn, &QPushButton::clicked, &dlg, [&]() {
        m_specificDate = QDate(year, month, day);
        m_useSpecificDate = true;
        dlg.accept();
    });
    connect(autoBtn, &QPushButton::clicked, &dlg, [&]() {
        m_useSpecificDate = false;
        dlg.accept();
    });
    connect(cancelBtn, &QPushButton::clicked, &dlg, &QDialog::reject);

    btns->addWidget(setBtn, 1);
    btns->addWidget(autoBtn, 1);
    btns->addWidget(cancelBtn, 1);
    root->addLayout(btns);

    refresh();
    if (dlg.exec() == QDialog::Accepted) {
        refreshDateSummary();
    }
}
