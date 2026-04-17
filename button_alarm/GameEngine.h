#pragma once

#include <QObject>
#include <QTimer>

// -------------------------------------------------------
// GameEngine
//   - 상태 머신: READY -> PLAYING -> SUCCESS
//   - 카운트 관리 및 30회 달성 감지
// -------------------------------------------------------
class GameEngine : public QObject
{
    Q_OBJECT

public:
    enum class State {
        READY,
        PLAYING,
        SUCCESS,
        FAILURE
    };
    Q_ENUM(State)

    static constexpr int GAME_DURATION  = 15;  // seconds
    static constexpr int TARGET_MIN      = 30;
    static constexpr int TARGET_MAX      = 60;

    explicit GameEngine(QObject *parent = nullptr);

    State state()       const { return m_state; }
    int   count()       const { return m_count; }
    int   targetCount() const { return m_targetCount; }

public slots:
    void startGame();    // READY -> PLAYING
    void onButtonPressed(); // 입력 1회 처리
    void restartGame();  // SUCCESS -> READY (→ startGame 호출)

signals:
    void stateChanged(GameEngine::State newState);
    void countUpdated(int count);
    void countdownUpdated(int secondsLeft);  // 10..1..0
    void gameSuccess();
    void gameFailure();

private slots:
    void onCountdownTick();

private:
    State  m_state       { State::READY };
    int    m_count       { 0 };
    int    m_targetCount { 30 };
    int    m_secondsLeft { GAME_DURATION };
    QTimer m_countdownTimer;

    void setState(State s);
};
