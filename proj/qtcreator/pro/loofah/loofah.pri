
TARGET = loofah
TEMPLATE = lib
VERSION = 1.0.0

include(../loofah_common.pri)

QT -= qt

DEFINES += BUILDING_LOOFAH

# INCLUDE 路径
SRC_ROOT = $$PWD/../../../../src/loofah
INCLUDEPATH += $${SRC_ROOT}/..

# 头文件
HEADERS += $$files($${SRC_ROOT}/*.h*, true)

# 源代码
SOURCES += $$files($${SRC_ROOT}/*.c*, true)

# nut
INCLUDEPATH += $${NUT_PATH}/src
LIBS += -L$$OUT_PWD/../nut$${OUT_TAIL}
win32: LIBS += -lnut1
else: LIBS += -lnut

# 连接库
win32: {
    LIBS += -latomic

    # NOTE 这些 win 网络库必须放在最后，否则会出错
    #      See http://stackoverflow.com/questions/2033608/mingw-linker-error-winsock
    LIBS += -lwininet -lws2_32 -lwsock32
}
