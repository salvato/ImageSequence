QT += core
QT += gui
QT += widgets

TARGET = ImageSequence
TEMPLATE = app

DEFINES += QT_DEPRECATED_WARNINGS
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

CONFIG += c++14

SOURCES += main.cpp
SOURCES += mainwindow.cpp
SOURCES += setupdialog.cpp

HEADERS += mainwindow.h
HEADERS += setupdialog.h

FORMS += mainwindow.ui
FORMS += setupdialog.ui

INCLUDEPATH += /usr/local/include
LIBS += -L"/usr/local/lib" -lpigpiod_if2


# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

DISTFILES += \
    movie.png
