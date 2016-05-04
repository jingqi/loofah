
TARGET = loofah
TEMPLATE = lib

QT -= core gui
CONFIG += staticlib

# 配置输出目录
DESTDIR = $$PWD/../..
NUT_OUTDIR = $$PWD/../../../../lib/nut.git/proj/qtcreator
mac {
    DESTDIR = $${DESTDIR}/mac
    NUT_OUTDIR = $${NUT_OUTDIR}/mac
} else: unix {
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
SRC_ROOT = $$PWD/../../../../src/loofah
INCLUDEPATH += \
    $$PWD/../../../../lib/nut.git/src \
    $${SRC_ROOT}/..

# 头文件
HEADERS += $$files($${SRC_ROOT}/*.h*, true)

# 源代码
SOURCES += $$files($${SRC_ROOT}/*.c*, true)

# 连接库
mac {
    LIBS += -lc++
} else: !unix {
    LIBS += libpthread -lwininet -lws2_32 -lwsock32
}
LIBS += -L$${NUT_OUTDIR} -lnut

# dylib 安装路径
mac: contains(TEMPLATE, lib): !contains(CONFIG, staticlib) {
    QMAKE_POST_LINK = install_name_tool -id @loader_path/lib$${TARGET}.dylib \
        $${DESTDIR}/lib$${TARGET}.dylib$$escape_expand(\n\t)
}
