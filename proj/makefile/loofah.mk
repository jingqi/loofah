#!/usr/bin/env make

# Preface
include ${NUT_PATH}/proj/makefile/mkinclude/vars.mk
include ${NUT_PATH}/proj/makefile/mkinclude/funcs.mk

# Project vars
TARGET_NAME = loofah
SRC_ROOT = ../../src/${TARGET_NAME}
OBJ_ROOT = ${OUT_DIR}/obj/${TARGET_NAME}
TARGET = ${OUT_DIR}/lib${TARGET_NAME}.${DL_SUFFIX}

# Make dirs
$(call make_image_dir_tree,${SRC_ROOT},${OBJ_ROOT})

# C/C++ standard
CFLAGS += -std=c11
CXXFLAGS += -std=c++11

# Defines and flags
CPPFLAGS += -DBUILDING_LOOFAH

# Includes
CPPFLAGS += -I${SRC_ROOT}/..

# Files
SRCS = $(call files,${SRC_ROOT},*.c *.cpp)
OBJS = $(patsubst ${SRC_ROOT}%.cpp,${OBJ_ROOT}%.o,$(patsubst ${SRC_ROOT}%.c,${OBJ_ROOT}%.o,${SRCS}))
DEPS = $(patsubst ${SRC_ROOT}%.cpp,${OBJ_ROOT}%.d,$(patsubst ${SRC_ROOT}%.c,${OBJ_ROOT}%.d,${SRCS}))

# Library nut
CPPFLAGS += -I${NUT_PATH}/src
LDFLAGS += -L${OUT_DIR} -lnut
LIB_DEPS += ${OUT_DIR}/libnut.${DL_SUFFIX}

# Other libraries
ifeq (${HOST}, Linux)
	LDFLAGS += -lpthread
endif

# Targets
.PHONY: all clean rebuild

all: ${TARGET}

clean:
	$(RM) ${OBJS} ${DEPS} ${TARGET}

rebuild:
	$(MAKE) -f loofah.mk clean
	$(MAKE) -f loofah.mk all

${NUT_PATH}/proj/makefile/${OUT_DIR_NAME}/libnut.${DL_SUFFIX}: FORCE
	cd ${NUT_PATH}/proj/makefile ; $(MAKE) -f nut.mk

${OUT_DIR}/libnut.${DL_SUFFIX}: ${NUT_PATH}/proj/makefile/${OUT_DIR_NAME}/libnut.${DL_SUFFIX}
	cp -f $< $@

# Rules
include ${NUT_PATH}/proj/makefile/mkinclude/common_rules.mk
include ${NUT_PATH}/proj/makefile/mkinclude/shared_lib_rules.mk
