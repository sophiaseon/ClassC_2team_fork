#-------------------------------------------------
#
# Project created by QtCreator 2025-02-21T17:42:11
#
#-------------------------------------------------

QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = camera
TEMPLATE = app


SOURCES += main.cpp\
        mainwindow.cpp \
    camerathread.cpp

HEADERS  += mainwindow.h \
    camerathread.h

FORMS    += mainwindow.ui
