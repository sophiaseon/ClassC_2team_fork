#include "gameengine.h"

#include <QDebug>
#include <QRandomGenerator>

GameEngine::GameEngine(QObject *parent)
    : QObject(parent)
{
    m_countdownTimer.setInterval(1000);
    connect(&m_countdownTimer, &QTimer::timeout, this, &GameEngine::onCountdownTick);

    m_successTimer.setSingleShot(true);
    connect(&m_successTimer, &QTimer::timeout, this, &GameEngine::onSuccessTimerFired);
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
    qDebug() << "[GameEngine] game started, target =" << m_targetCount;
}

void GameEngine::onButtonPressed()
{
    if (m_state != State::PLAYING) return;

    ++m_count;
    qDebug() << "[GameEngine] count =" << m_count;
    emit countUpdated(m_count);

    if (m_count > m_targetCount) {
        // overshoot: immediate failure
        m_countdownTimer.stop();
        m_successTimer.stop();
        setState(State::FAILURE);
        emit gameFailure();
        qDebug() << "[GameEngine] FAILURE - overshoot (" << m_count << ">" << m_targetCount << ")";
    } else if (m_count == m_targetCount) {
        // exact match: success after min(3s, remaining time)
        const int waitMs = (m_secondsLeft > 3) ? 3000 : (m_secondsLeft * 1000);
        if (waitMs > 0) {
            m_successTimer.start(waitMs);
        } else {
            // no time remaining: immediate success
            m_countdownTimer.stop();
            setState(State::SUCCESS);
            emit gameSuccess();
        }
        qDebug() << "[GameEngine] target hit - waiting" << waitMs << "ms for success";
    }
}

void GameEngine::onCountdownTick()
{
    --m_secondsLeft;
    emit countdownUpdated(m_secondsLeft);

    if (m_secondsLeft <= 0) {
        m_countdownTimer.stop();
        m_successTimer.stop();
        if (m_count == m_targetCount) {
            setState(State::SUCCESS);
            emit gameSuccess();
            qDebug() << "[GameEngine] SUCCESS at timeout (" << m_count << "==" << m_targetCount << ")";
        } else {
            setState(State::FAILURE);
            emit gameFailure();
            qDebug() << "[GameEngine] FAILURE at timeout (" << m_count << "/" << m_targetCount << ")";
        }
    }
}

void GameEngine::onSuccessTimerFired()
{
    if (m_state != State::PLAYING) return;
    m_countdownTimer.stop();
    setState(State::SUCCESS);
    emit gameSuccess();
    qDebug() << "[GameEngine] SUCCESS via success timer";
}

void GameEngine::restartGame()
{
    m_countdownTimer.stop();
    m_successTimer.stop();
    m_count = 0;
    m_targetCount = 0;
    m_secondsLeft = GAME_DURATION;
    emit countUpdated(m_count);
    setState(State::READY);
    startGame();
}
