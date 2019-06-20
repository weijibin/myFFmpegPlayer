#-------------------------------------------------
#
# Project created by QtCreator 2019-06-20T15:44:38
#
#-------------------------------------------------

QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = MyFFMPEGTest
TEMPLATE = app


SOURCES += main.cpp\
        widget.cpp \
    sdlWidget/sdl_renderwnd.cpp

HEADERS  += widget.h \
    sdlWidget/sdl_renderwnd.h

FORMS    += widget.ui

INCLUDEPATH += $$PWD/3rdPart/ffmpeg/include
INCLUDEPATH += $$PWD/3rdPart/sdl2/include

#LIBS += \
#    $$PWD/3rdPart/ffmpeg/lib/avcodec.lib \
#    $$PWD/3rdPart/ffmpeg/lib/avdevice.lib \
#    $$PWD/3rdPart/ffmpeg/lib/avfilter.lib \
#    $$PWD/3rdPart/ffmpeg/lib/avformat.lib \
#    $$PWD/3rdPart/ffmpeg/lib/avutil.lib \
#    $$PWD/3rdPart/ffmpeg/lib/postproc.lib \
#    $$PWD/3rdPart/ffmpeg/lib/swscale.lib
LIBS += -L$$PWD/3rdPart/ffmpeg/lib \
    -lavcodec \
    -lavdevice \
    -lavfilter \
    -lavformat \
    -lavutil \
    -lpostproc \
    -lswscale

LIBS += -L$$PWD/3rdPart/sdl2/lib/x86 \
    -lSDL2

RESOURCES +=


