
TARGET = test_loofah
TEMPLATE = app

QT -= core gui
CONFIG += console
CONFIG -= app_bundle

# 配置输出目录
DESTDIR = $$PWD/../..
NUT_OUTDIR = $$PWD/../../../../lib/nut.git/proj/qtcreator
mac {
    DESTDIR = $${DESTDIR}/mac
    NUT_OUTDIR = $${NUT_OUTDIR}/mac
} else : unix {
    DESTDIR = $${DESTDIR}/unix
    NUT_OUTDIR = $${NUT_OUTDIR}/unix
} else {
    DESTDIR = $${DESTDIR}/win
    NUT_OUTDIR = $${NUT_OUTDIR}/win
}
DESTDIR = $${DESTDIR}-$${QMAKE_HOST.arch}
NUT_OUTDIR = $${NUT_OUTDIR}-$${QMAKE_HOST.arch}
CONFIG(debug, debug|release) {
    DESTDIR = $${DESTDIR}-debug
    NUT_OUTDIR = $${NUT_OUTDIR}-debug
} else {
    DESTDIR = $${DESTDIR}-release
    NUT_OUTDIR = $${NUT_OUTDIR}-release
}
message("DESTDIR: "$${DESTDIR})

# C++11 支持
QMAKE_CXXFLAGS += -std=c++11
mac {
    QMAKE_CXXFLAGS += -stdlib=libc++
}

# 这里貌似是qmake的一个bug，不会主动添加 _DEBUG/NDEBUG 宏
CONFIG(debug, debug|release) {
    DEFINES += _DEBUG
} else {
    DEFINES += NDEBUG
}

# INCLUDE 路径
SRC_ROOT = $$PWD/../../../../src/test_loofah
INCLUDEPATH += \
    $$PWD/../../../../lib/nut.git/src \
    $${SRC_ROOT}/.. \
    $${SRC_ROOT}

# 源代码
SOURCES += $$files($${SRC_ROOT}/*.c*, true)

# 连接库
LIBS += -L$${NUT_OUTDIR} -lnut
LIBS += -L$${DESTDIR} -lloofah
mac {
    LIBS += -lc++
} else: unix {
    LIBS += -lrt
} else {
    LIBS += -lpthread
    LIBS += -lws2_32 -lwininet -lwsock32 -lmswsock # NOTE 这些 win 网络库必须放在最后，否则会出错
        # see http://stackoverflow.com/questions/2033608/mingw-linker-error-winsock
}
