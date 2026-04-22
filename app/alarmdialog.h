#ifndef ALARMDIALOG_H
#define ALARMDIALOG_H

#include <QDate>
#include <QDateTime>
#include <QDialog>
#include <QElapsedTimer>
#include <functional>

class QComboBox;
class QLabel;
class QPushButton;
class QStackedWidget;
class QTimer;
class QWidget;

class AlarmDialog : public QDialog
{
    Q_OBJECT

public:
    enum DismissMode {
        DismissSimple = 0,
        DismissGame = 1,
        DismissButton = 2,
        DismissCamera = 3
    };

    enum GameType {
        GameNumberOrder = 0,
        GameColorMemory = 1,
        GameUltrasonic  = 2
    };

    explicit AlarmDialog(QWidget *parent = nullptr,
                         int editIndex = -1,
                         const QDateTime &initialDt = QDateTime(),
                         const QString &initialSound = QString(),
                         int initialDismissMode = DismissSimple,
                         int initialGameType = GameNumberOrder,
                         int initialRepeatMask = 0,
                         bool initialUseSpecificDate = false);

    QDateTime selectedDateTime() const;
    QString soundFile() const;
    int dismissMode() const;
    int gameType() const;
    int repeatMask() const;
    bool useSpecificDate() const;
    int editIndex() const { return m_editIndex; }

private slots:
    void onConfirm();
    void increaseAmPm();
    void decreaseAmPm();
    void increaseHour();
    void decreaseHour();
    void increaseMinute();
    void decreaseMinute();
    void openCalendarDialog();

private:
    void buildUi();
    void connectRepeatButton(QPushButton *btn, std::function<void()> action);
    QTime selectedTime() const;
    void setTimeFromDateTime(const QDateTime &dt);
    void refreshTimeLabels();
    void setRepeatButtonsFromMask(int mask);
    int repeatMaskFromButtons() const;
    void refreshModeStyle();
    void refreshDateSummary();
    QDateTime computeNextForRepeat(const QDateTime &now, const QTime &time, int mask) const;

    int m_editIndex;

    QDateTime m_result;
    QString m_soundResult;
    int m_dismissMode;
    int m_gameType;
    int m_repeatMask;
    bool m_useSpecificDate;
    QDate m_specificDate;

    int m_ampm;   // 0=AM, 1=PM
    int m_hour12; // 1..12
    int m_minute; // 0..59

    QLabel *m_ampmValueLabel;
    QLabel *m_hourValueLabel;
    QLabel *m_minuteValueLabel;

    QPushButton *m_ampmPlusButton;
    QPushButton *m_ampmMinusButton;
    QPushButton *m_hourPlusButton;
    QPushButton *m_hourMinusButton;
    QPushButton *m_minutePlusButton;
    QPushButton *m_minuteMinusButton;

    QPushButton *m_weekdayButtons[7]; // 0=Sun ... 6=Sat
    bool         m_weekdayLocked[7];  // per-button debounce lock

    QLabel *m_dateSummaryLabel;
    QPushButton *m_calendarToggleBtn;

    QElapsedTimer m_adjustTimer;

    QTimer *m_repeatTimer;
    std::function<void()> m_repeatAction;
    int m_repeatCount;

    QComboBox *m_soundCombo;
    QComboBox *m_gameCombo;
    QStackedWidget *m_gameOptionStack;
    QPushButton *m_simpleModeBtn;
    QPushButton *m_gameModeBtn;
    QPushButton *m_buttonModeBtn;
    QPushButton *m_cameraModeBtn;
};

#endif // ALARMDIALOG_H
