#pragma once

#include <QObject>
#include <QTimer>

// -------------------------------------------------------
// GameEngine
//   - state machine: READY -> PLAYING -> SUCCESS / FAILURE
//   - press the button TARGET times within GAME_DURATION seconds to SUCCESS
//   - too few or too many presses → FAILURE; retry with restartGame()
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

    static constexpr int GAME_DURATION = 15;  // seconds
    static constexpr int TARGET_MIN    = 30;
    static constexpr int TARGET_MAX    = 60;

    explicit GameEngine(QObject *parent = nullptr);

    State state()       const { return m_state; }
    int   count()       const { return m_count; }
    int   targetCount() const { return m_targetCount; }

public slots:
    void startGame();
    void onButtonPressed();
    void restartGame();

signals:
    void stateChanged(GameEngine::State newState);
    void countUpdated(int count);
    void countdownUpdated(int secondsLeft);
    void gameSuccess();
    void gameFailure();

private slots:
    void onCountdownTick();
    void onSuccessTimerFired();

private:
    State  m_state       { State::READY };
    int    m_count       { 0 };
    int    m_targetCount { TARGET_MIN };
    int    m_secondsLeft { GAME_DURATION };
    QTimer m_countdownTimer;
    QTimer m_successTimer;  // fires after min(3s, remaining) when exact target reached

    void setState(State s);
};
