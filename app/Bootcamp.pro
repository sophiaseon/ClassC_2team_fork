QT += core gui widgets concurrent

CONFIG += c++11

TEMPLATE = app
TARGET = Bootcamp

SOURCES += \
    alarmdialog.cpp \
    alarmcamerathread.cpp \
    buttonwatcher.cpp \
    dismissdialog.cpp \
    gameengine.cpp \
    main.cpp \
    mainwindow.cpp \
    statdialog.cpp

HEADERS += \
    alarmdialog.h \
    alarmcamerathread.h \
    buttonwatcher.h \
    dismissdialog.h \
    gameengine.h \
    mainwindow.h \
    statdialog.h
