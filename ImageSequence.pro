#-------------------------------------------------
#
# Project created by QtCreator 2019-09-02T15:48:09
#
#-------------------------------------------------

QT += core
QT += gui
QT += widgets

TARGET = ImageSequence
TEMPLATE = app

DEFINES += QT_DEPRECATED_WARNINGS
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

CONFIG += c++14

SOURCES += main.cpp \
    setupdialog.cpp
SOURCES += mainwindow.cpp

HEADERS += mainwindow.h \
    setupdialog.h

FORMS += mainwindow.ui \
    setupdialog.ui

contains(QMAKE_HOST.arch, "armv7l") || contains(QMAKE_HOST.arch, "armv6l"): {
    INCLUDEPATH += /usr/local/include
    LIBS += -L"/usr/local/lib" -lpigpiod_if2 # To include libpigpiod_if2.so from /usr/local/lib
}

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
