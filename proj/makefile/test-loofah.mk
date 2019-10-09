#!/usr/bin/env make

# Preface
include ${NUT_PATH}/proj/makefile/mkinclude/vars.mk
include ${NUT_PATH}/proj/makefile/mkinclude/funcs.mk

# Project vars
TARGET_NAME = test-loofah
SRC_ROOT = ../../src/${TARGET_NAME}
OBJ_ROOT = ${OUT_DIR}/obj/${TARGET_NAME}
TARGET = ${OUT_DIR}/${TARGET_NAME}

# Make dirs
$(call make_image_dir_tree,${SRC_ROOT},${OBJ_ROOT})

# C/C++ standard
CFLAGS += -std=c11
CXXFLAGS += -std=c++11

# Defines and flags

# Includes

# Files
SRCS = $(call files,${SRC_ROOT},*.c *.cpp)
OBJS = $(patsubst ${SRC_ROOT}%.cpp,${OBJ_ROOT}%.o,$(patsubst ${SRC_ROOT}%.c,${OBJ_ROOT}%.o,${SRCS}))
DEPS = $(patsubst ${SRC_ROOT}%.cpp,${OBJ_ROOT}%.d,$(patsubst ${SRC_ROOT}%.c,${OBJ_ROOT}%.d,${SRCS}))

# Library loofah
CPPFLAGS += -I${SRC_ROOT}/..
LDFLAGS += -L${OUT_DIR} -lloofah
LIB_DEPS += ${OUT_DIR}/libloofah.${DL_SUFFIX}

# Library nut
CPPFLAGS += -I${NUT_PATH}/src
LDFLAGS += -L${OUT_DIR} -lnut
LIB_DEPS += ${OUT_DIR}/libnut.${DL_SUFFIX}

# Other libraries
ifeq (${HOST}, Linux)
	LDFLAGS += -lpthread -latomic
endif

# Targets
.PHONY: all clean rebuild

all: ${TARGET}

clean:
	$(RM) ${OBJS} ${DEPS} ${TARGET}

rebuild:
	$(MAKE) -f test-loofah.mk clean
	$(MAKE) -f test-loofah.mk all

${OUT_DIR}/libloofah.${DL_SUFFIX}: FORCE
	$(MAKE) -f loofah.mk

${NUT_PATH}/proj/makefile/${OUT_DIR_NAME}/libnut.${DL_SUFFIX}: FORCE
	cd ${NUT_PATH}/proj/makefile ; $(MAKE) -f nut.mk

${OUT_DIR}/libnut.${DL_SUFFIX}: ${NUT_PATH}/proj/makefile/${OUT_DIR_NAME}/libnut.${DL_SUFFIX}
	cp -f $< $@

# Rules
include ${NUT_PATH}/proj/makefile/mkinclude/common_rules.mk
include ${NUT_PATH}/proj/makefile/mkinclude/app_rules.mk
