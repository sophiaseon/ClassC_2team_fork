#ifndef DISMISSDIALOG_H
#define DISMISSDIALOG_H

#include <QDialog>
#include <QList>
#include <QStringList>

class AlarmCameraThread;
class QLabel;
class QPushButton;

class DismissDialog : public QDialog
{
    Q_OBJECT

public:
    enum Mode { Simple, Game, Button, Camera };
    enum GameType { NumberOrder = 0, ColorMemory = 1 };

    explicit DismissDialog(const QStringList &alarmTimes,
                           Mode mode,
                           GameType gameType = NumberOrder,
                           QWidget *parent = nullptr,
                           int alarmId = -1);
    ~DismissDialog() override;

    QString capturedPhotoPath() const;

public slots:
    void dismiss(); // called by MainWindow when physical button pressed
    void captureByButton();

protected:
    void closeEvent(QCloseEvent *event) override;
    void reject() override;

private slots:
    void onNumberClicked(int number);
    void onColorClicked(int colorIndex);

private:
    void buildSimpleUi(const QStringList &alarmTimes);
    void buildGameUi(const QStringList &alarmTimes);
    void buildNumberOrderGameUi(const QStringList &alarmTimes);
    void buildColorMemoryGameUi(const QStringList &alarmTimes);
    void buildButtonUi(const QStringList &alarmTimes);
    void buildCameraUi(const QStringList &alarmTimes);

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
};

#endif // DISMISSDIALOG_H
