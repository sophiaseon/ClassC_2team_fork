#ifndef DISMISSDIALOG_H
#define DISMISSDIALOG_H

#include <QDialog>
#include <QElapsedTimer>
#include <QList>
#include <QStringList>
#include <QVector>

#include "gameengine.h"

class AlarmCameraThread;
class UltrasonicWatcher;
class QLabel;
class QPushButton;

class DismissDialog : public QDialog
{
    Q_OBJECT

public:
    enum Mode { Simple, Game, Button, Camera, Ultrasonic };
    enum GameType { NumberOrder = 0, ColorMemory = 1 };

    explicit DismissDialog(const QStringList &alarmTimes,
                           Mode mode,
                           GameType gameType = NumberOrder,
                           QWidget *parent = nullptr,
                           int alarmId = -1);
    ~DismissDialog() override;

    QString capturedPhotoPath() const;

public slots:
    void dismiss(); // called by MainWindow when physical button pressed (Camera mode)
    void onButtonPressedForGame(); // called by MainWindow for Button game mode
    void captureByButton();

protected:
    void closeEvent(QCloseEvent *event) override;
    void reject() override;

private slots:
    void onNumberClicked(int number);
    void onColorClicked(int colorIndex);
    void onButtonGameCountUpdated(int count);
    void onButtonGameCountdownUpdated(int secondsLeft);
    void onButtonGameSuccess();
    void onButtonGameFailure();
    void onUltrasonicDistances(QVector<int> distances);

private:
    void buildSimpleUi(const QStringList &alarmTimes);
    void buildGameUi(const QStringList &alarmTimes);
    void buildNumberOrderGameUi(const QStringList &alarmTimes);
    void buildColorMemoryGameUi(const QStringList &alarmTimes);
    void buildButtonUi(const QStringList &alarmTimes);
    void buildCameraUi(const QStringList &alarmTimes);
    void buildUltrasonicUi(const QStringList &alarmTimes);
    void usAdvanceToStep(int step);  // update UI + state for next us step

    void startColorMemoryRound();
    void showColorMemoryStep();
    void setPreviewColor(int colorIndex, bool active);

    Mode         m_mode;
    GameType     m_gameType;

    int          m_nextExpected;         // for NumberOrder: next number to click
    QPushButton *m_numButtons[25];       // indexed by (number - 1)
    QLabel      *m_progressLabel;

    QPushButton *m_colorButtons[4];
    QLabel      *m_colorStatusLabel;
    QLabel      *m_colorPreviewLabel;
    QList<int>   m_colorSequence;
    int          m_colorRound;
    int          m_colorShowIndex;
    int          m_colorInputIndex;
    bool         m_showingSequence;

    int                m_alarmId;
    AlarmCameraThread *m_cameraThread;
    QLabel            *m_cameraStatusLabel;
    QLabel            *m_cameraPreviewLabel;
    bool               m_captureRequested;
    QString            m_capturedPhotoPath;

    // Button game mode
    GameEngine        *m_gameEngine;
    QLabel            *m_btnCountLabel;      // current press count
    QLabel            *m_btnTargetLabel;     // "N / target"
    QLabel            *m_btnCountdownLabel;  // seconds left
    QLabel            *m_btnStatusLabel;     // status message

    // Ultrasonic dismiss mode
    UltrasonicWatcher *m_usWatcher;
    int                m_usSeq[4];           // target sensor index (0-3) for each of the 4 steps
    int                m_usStep;             // current step (0-3)
    bool               m_usNeedRelease;      // must remove hand before hold counts
    bool               m_usHolding;          // hand is currently near the sensor
    QElapsedTimer      m_usHoldTimer;        // measures continuous hold duration
    QLabel            *m_usTargetLabel;      // big sensor number display
    QLabel            *m_usProgressLabel;    // "Step X / 4"
    QLabel            *m_usStatusLabel;      // instruction / status text
    QLabel            *m_usStepLabels[4];    // sequence boxes (highlight current)
    QLabel            *m_usDistLabels[4];    // live distance readouts

    QElapsedTimer      m_actionTimer;
};

#endif // DISMISSDIALOG_H
