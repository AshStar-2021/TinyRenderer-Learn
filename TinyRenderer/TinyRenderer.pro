QT       += core gui    #QT包含的模块

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets     #大于4版本，可使用widget模块

CONFIG += c++17

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    geometry.cpp \
    main.cpp \
    mainwindow.cpp \
    model.cpp \
    our_gl.cpp \
    tgaimage.cpp

HEADERS += \
    geometry.h \
    mainwindow.h \
    model.h \
    our_gl.h \
    tgaimage.h

FORMS += \
    mainwindow.ui

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
