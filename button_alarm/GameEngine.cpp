#include "GameEngine.h"
#include <QDebug>
#include <QRandomGenerator>

GameEngine::GameEngine(QObject *parent)
    : QObject(parent)
{
    m_countdownTimer.setInterval(1000);
    connect(&m_countdownTimer, &QTimer::timeout, this, &GameEngine::onCountdownTick);
}

void GameEngine::setState(State s)
{
    if (m_state == s) return;
    m_state = s;
    emit stateChanged(m_state);
}

void GameEngine::startGame()
{
    if (m_state != State::READY) return;

    m_targetCount = QRandomGenerator::global()->bounded(TARGET_MIN, TARGET_MAX + 1);
    m_count = 0;
    m_secondsLeft = GAME_DURATION;
    emit countUpdated(m_count);
    emit countdownUpdated(m_secondsLeft);
    setState(State::PLAYING);
    m_countdownTimer.start();
    qDebug() << "GameEngine: game started, target =" << m_targetCount;
}

void GameEngine::onButtonPressed()
{
    if (m_state != State::PLAYING) return;

    ++m_count;
    qDebug() << "GameEngine: count =" << m_count;
    emit countUpdated(m_count);
}

void GameEngine::onCountdownTick()
{
    --m_secondsLeft;
    emit countdownUpdated(m_secondsLeft);
    qDebug() << "GameEngine: countdown =" << m_secondsLeft;

    if (m_secondsLeft <= 0) {
        m_countdownTimer.stop();
        if (m_count == m_targetCount) {
            setState(State::SUCCESS);
            emit gameSuccess();
            qDebug() << "GameEngine: SUCCESS (" << m_count << "==" << m_targetCount << ")";
        } else {
            setState(State::FAILURE);
            emit gameFailure();
            qDebug() << "GameEngine: FAILURE (" << m_count << "/" << m_targetCount << ")";
        }
    }
}

void GameEngine::restartGame()
{
    m_countdownTimer.stop();
    m_count = 0;
    m_targetCount = 0;
    m_secondsLeft = GAME_DURATION;
    emit countUpdated(m_count);
    setState(State::READY);
    startGame();
}
