
TARGET = loofah
TEMPLATE = lib

QT -= core gui
CONFIG += staticlib

# 配置输出目录
DESTDIR = $$PWD/../..
mac {
    DESTDIR = $${DESTDIR}/mac
} else: unix {
    DESTDIR = $${DESTDIR}/unix
} else {
    DESTDIR = $${DESTDIR}/win
}
DESTDIR = $${DESTDIR}-$${QMAKE_HOST.arch}
CONFIG(debug, debug|release) {
    DESTDIR = $${DESTDIR}-debug
} else {
    DESTDIR = $${DESTDIR}-release
}
message("DESTDIR: "$${DESTDIR})

# C++11 支持
QMAKE_CXXFLAGS += -std=c++11
QMAKE_CFLAGS += -std=c++11

# 这里貌似是qmake的一个bug，不会主动添加 _DEBUG/NDEBUG 宏
CONFIG(debug, debug|release) {
    DEFINES += _DEBUG
} else {
    DEFINES += NDEBUG
}

# INCLUDE 路径
SRC_ROOT = $$PWD/../../../../src/loofah
INCLUDEPATH += \
    $${SRC_ROOT}/..

# 头文件
HEADERS +=\
    $$files($${SRC_ROOT}/*.h*)

# 源代码
SOURCES +=\
    $$files($${SRC_ROOT}/*.c*)

# 连接库
!unix {
    LIBS += -lpthread
}
