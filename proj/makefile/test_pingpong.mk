#!/user/bin/env make

TARGET_NAME = test_pingpong
SRC_ROOT = ../../src/${TARGET_NAME}

# preface rules
NUT_MAKEFILE_DIR = $(CURDIR)/../../lib/nut.git/proj/makefile
include ${NUT_MAKEFILE_DIR}/preface_rules.mk

# variables
ifeq (${DEBUG}, 1)
	NUT_OUT_DIR = ${NUT_MAKEFILE_DIR}/debug
else
	NUT_OUT_DIR = ${NUT_MAKEFILE_DIR}/release
endif

# INC
INC += -I../../lib/nut.git/src -I${SRC_ROOT}/..

# DEF
DEF +=

# CXX_FLAGS
CXX_FLAGS += -std=c++11

# LIB
ifeq (${HOST}, Darwin)
	LIB_NUT = ${NUT_OUT_DIR}/libnut.dylib
	LIB_NUT_DUP = ${OUT_DIR}/libnut.dylib
	LIB_LOOFAH = ${OUT_DIR}/libloofah.dylib
else
	LIB += -lpthread
	LIB_NUT = ${NUT_OUT_DIR}/libnut.so
	LIB_NUT_DUP = ${OUT_DIR}/libnut.so
	LIB_LOOFAH = ${OUT_DIR}/libloofah.so
endif
LIB += -L${OUT_DIR} -lnut -lloofah
LIB_DEPS += ${LIB_NUT_DUP} ${LIB_LOOFAH}

# LD_FLAGS
LD_FLAGS +=

# TARGET
TARGET = ${OUT_DIR}/${TARGET_NAME}

.PHONY: all clean rebuild

all: ${TARGET}

clean:
	$(MAKE) -f loofah.mk clean
	rm -rf ${OBJS}
	rm -rf ${DEPS}
	rm -rf ${TARGET}

rebuild:
	$(MAKE) -f test_loofah.mk clean
	$(MAKE) -f test_loofah.mk all

${LIB_NUT_DUP}: ${LIB_NUT}
	cp -f $< $@

${LIB_NUT}: FORCE
	cd ${NUT_MAKEFILE_DIR} ; $(MAKE) -f nut.mk

${LIB_LOOFAH}: ${LIB_NUT_DUP} FORCE
	$(MAKE) -f loofah.mk

# rules
include ${NUT_MAKEFILE_DIR}/common_rules.mk
include ${NUT_MAKEFILE_DIR}/app_rules.mk
