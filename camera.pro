TEMPLATE = app
TARGET = camera

QT += multimedia multimediawidgets

HEADERS = \
    camera.h \
    imagesettings.h \
    videosettings.h \
    osclistener.h \
    graphrepainter.h \
    musedatastream.h

SOURCES = \
    main.cpp \
    camera.cpp \
    imagesettings.cpp \
    videosettings.cpp

FORMS += \
    camera.ui \
    videosettings.ui \
    imagesettings.ui

INCLUDEPATH += /Users/kevinatkinson/Downloads/oscpack_1_1_0

target.path = $$[QT_INSTALL_EXAMPLES]/multimediawidgets/camera
INSTALLS += target

LIBS += -L/usr/local/lib -loscpack

QT+=widgets
