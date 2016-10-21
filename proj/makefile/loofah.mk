#!/user/bin/env make

TARGET_NAME = loofah
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
DEF += -DBUILD_LOOFAH_DLL

# CXX_FLAGS
CXX_FLAGS += -std=c++11

# LIB LIB_DEPS
ifeq (${HOST}, Darwin)
	LIB_NUT += ${NUT_OUT_DIR}/libnut.dylib
	LIB_NUT_DUP += ${OUT_DIR}/libnut.dylib
else
	LIB += -lpthread
	LIB_NUT += ${NUT_OUT_DIR}/libnut.so
	LIB_NUT_DUP += ${OUT_DIR}/libnut.so
endif
LIB += -L${OUT_DIR} -lnut
LIB_DEPS += ${LIB_NUT_DUP}

# LD_FLAGS
LD_FLAGS +=

# TARGET
ifeq (${HOST}, Darwin)
	TARGET = ${OUT_DIR}/lib${TARGET_NAME}.dylib
else
	TARGET = ${OUT_DIR}/lib${TARGET_NAME}.so
endif

.PHONY: all clean rebuild

all: ${TARGET}

clean:
	cd ${NUT_MAKEFILE_DIR} ; $(MAKE) -f nut.mk
	rm -rf ${OBJS}
	rm -rf ${DEPS}
	rm -rf ${TARGET}

rebuild:
	$(MAKE) -f loofah.mk clean
	$(MAKE) -f loofah.mk all

${LIB_NUT_DUP}: ${LIB_NUT}
	cp -f $< $@

${LIB_NUT}: FORCE
	cd ${NUT_MAKEFILE_DIR} ; $(MAKE) -f nut.mk

# rules
include ${NUT_MAKEFILE_DIR}/common_rules.mk
include ${NUT_MAKEFILE_DIR}/shared_rules.mk
