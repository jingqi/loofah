
TARGET = test_pingpong
TEMPLATE = app

include(../loofah_common.pri)

QT -= qt
CONFIG += console
CONFIG -= app_bundle

# INCLUDE 路径
SRC_ROOT = $$PWD/../../../../src/test_pingpong
INCLUDEPATH += $${SRC_ROOT}

# 头文件
HEADERS += $$files($${SRC_ROOT}/*.h*, true)

# 源代码
SOURCES += $$files($${SRC_ROOT}/*.c*, true)

# loofah
INCLUDEPATH += $$PWD/../../../../src
LIBS += -L$$OUT_PWD/../loofah$${OUT_TAIL}
win32: LIBS += -lloofah1
else: LIBS += -lloofah

# nut
INCLUDEPATH += $$PWD/../../../../lib/nut.git/src
LIBS += -L$$OUT_PWD/../nut$${OUT_TAIL}
win32: LIBS += -lnut1
else: LIBS += -lnut

# 连接库
win32: {
    # NOTE 这些 win 网络库必须放在最后，否则会出错
    #      See http://stackoverflow.com/questions/2033608/mingw-linker-error-winsock
    LIBS += -lws2_32 -lwininet -lwsock32 -lmswsock
}
