######################################################################
# Automatically generated by qmake (2.01a) Sat Jun 14 09:11:56 2008
######################################################################

TEMPLATE = app
TARGET = StimulateOpenGL_II
DEPENDPATH += .
INCLUDEPATH += . SFMT

CONFIG += qt thread warn_on
QT += core network gui opengl
greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

# Input
HEADERS +=  Version.h StimApp.h Util.h RNG.h ConsoleWindow.h GLHeaders.h \
            GLWindow.h ConnectionThread.h StimPlugin.h CalibPlugin.h \
            MovingObjects_Old.h MovingGrating.h CheckerFlicker.h \
            ZigguratGauss.h StimParams.h StimGL_SpikeGL_Integration.h \
            FrameVariables.h Flicker.h Flicker_RGBW.h Sawtooth.h DAQ.h \
            TypeDefs.h Shapes.h MovingObjects.h Movie.h GifReader.h \
            FastMovieFormat.h FastMovieReader.h GLBoxSelector.h
SOURCES +=  main.cpp StimApp.cpp Util.cpp RNG.cpp ConsoleWindow.cpp \
            GLWindow.cpp osdep.cpp ConnectionThread.cpp \
            StimPlugin.cpp CalibPlugin.cpp MovingObjects_Old.cpp \
            MovingGrating.cpp CheckerFlicker.cpp ZigguratGauss.cpp \
            StimParams.cpp StimGL_SpikeGL_Integration.cpp FrameVariables.cpp \
            Flicker.cpp Flicker_RGBW.cpp Sawtooth.cpp DAQ.cpp Shapes.cpp \
            MovingObjects.cpp Movie.cpp GifReader.cpp FastMovieFormat.cpp \
            FastMovieReader.cpp GLBoxSelector.cpp

FORMS += SpikeGLIntegration.ui ParamDefaultsWindow.ui

unix {
        LIBS += -lm
        DEFINES += UNIX
        QMAKE_CFLAGS_DEBUG += -msse2
        QMAKE_CXXFLAGS_DEBUG += -msse2
        QMAKE_CFLAGS_RELEASE += -msse2 -march=pentium4 -O3
        QMAKE_CXXFLAGS_RELEASE += -msse2 -march=pentium4 -O3
        QMAKE_CFLAGS_WARN_ON += -Wno-unused-private-field -Wno-deprecated-declarations -Wno-unused-function
        QMAKE_CXXFLAGS_WARN_ON += -Wno-unused-private-field -Wno-deprecated-declarations -Wno-unused-function
}
macx {
        LIBS += -framework CoreServices
        DEFINES += MACX
        QMAKE_CFLAGS_WARN_ON += # how to turn off #warnings messages?
        QMAKE_CXXFLAGS_WARN_ON +=  # how to turn off #warnings messages?
}
win32-g++ {
        QMAKE_CFLAGS_DEBUG += -msse2
        QMAKE_CXXFLAGS_DEBUG += -msse2
        QMAKE_CFLAGS_RELEASE += -msse2 -march=pentium4 -O3
        QMAKE_CXXFLAGS_RELEASE += -msse2 -march=pentium4 -O3
        LIBS += -lm
}
win32 {
        QMAKE_CFLAGS_DEBUG += -D_WIN32_WINNT=0x0400
        QMAKE_CFLAGS_RELEASE += -D_WIN32_WINNT=0x0400
        QMAKE_CXXFLAGS_DEBUG += -D_WIN32_WINNT=0x0400
        QMAKE_CXXFLAGS_RELEASE += -D_WIN32_WINNT=0x0400
        LIBS += $${PWD}/NI/NIDAQmx.lib WS2_32.lib DelayImp.lib
        DEFINES += HAVE_NIDAQmx WIN32
        RC_FILE += WinResources.rc
        QMAKE_CFLAGS_RELEASE -= /O2 /O1 -O1 -O2
        QMAKE_CXXFLAGS_RELEASE -= /O2 /O1 -O1 -O2
        QMAKE_CFLAGS_RELEASE += -arch:SSE2 -Ox
        QMAKE_CXXFLAGS_RELEASE += -arch:SSE2 -Ox
        QMAKE_LFLAGS += /DELAYLOAD:"nicaiu.dll"

        greaterThan(QT_MAJOR_VERSION, 4) {
            LIBS += opengl32.lib GDI32.lib GLU32.lib user32.lib
#            QMAKE_LFLAGS += /VERBOSE:LIB
        }

        contains(QMAKE_TARGET.arch, x86_64) {
            DEFINES += WIN64
            LIBS -= $${PWD}/NI/NIDAQmx.lib
            LIBS += $${PWD}/NI/x64/NIDAQmx.lib
            QMAKE_CFLAGS_RELEASE -= -arch:SSE2
            QMAKE_CXXFLAGS_RELEASE -= -arch:SSE2
        }
}

