//#ifndef MAINWINDOW_H
//#define MAINWINDOW_H

//#include <QMainWindow>

//QT_BEGIN_NAMESPACE
//namespace Ui { class MainWindow; }
//QT_END_NAMESPACE

//class MainWindow : public QMainWindow
//{
//    Q_OBJECT

//public:
//    MainWindow(QWidget *parent = nullptr);
//    ~MainWindow();

//private:
//    Ui::MainWindow *ui;
//};
//#endif // MAINWINDOW_H


#pragma once

#include <QMainWindow>
#include <QStackedWidget>
#include <QLabel>
#include <QPushButton>
#include <QKeyEvent>

#include "GameEngine.h"
#include "ButtonHandler.h"

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override = default;

protected:
    void keyPressEvent(QKeyEvent *event) override;

private slots:
    void onStateChanged(GameEngine::State state);
    void onCountUpdated(int count);
    void onCountdownUpdated(int secondsLeft);
    void onDeviceError(const QString &message);

private:
    GameEngine    *m_engine  { nullptr };
    ButtonHandler *m_button  { nullptr };

    QStackedWidget *m_stack  { nullptr };

    QWidget *m_readyPage   { nullptr };

    QWidget *m_playPage           { nullptr };
    QLabel  *m_countLabel         { nullptr };  // press count
    QLabel  *m_progressLabel      { nullptr };  // "N / 30"
    QLabel  *m_countdownLabel     { nullptr };  // "10".."1"

    QWidget *m_successPage        { nullptr };
    QWidget *m_failurePage        { nullptr };
    QLabel  *m_successResultLabel { nullptr };  // "actual / target"
    QLabel  *m_failureResultLabel { nullptr };  // "actual / target"

    QWidget *buildReadyPage();
    QWidget *buildPlayingPage();
    QWidget *buildSuccessPage();
    QWidget *buildFailurePage();

    void applyGlobalStyle();
    void showPage(int index);
};
