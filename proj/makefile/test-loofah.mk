#!/usr/bin/env make

# Preface
include ${NUT_PATH}/proj/makefile/mkinclude/vars.mk
include ${NUT_PATH}/proj/makefile/mkinclude/funcs.mk

# Project vars
TARGET_NAME = test-loofah
SRC_ROOT = ../../src/${TARGET_NAME}
OBJ_ROOT = ${OUT_DIR}/obj/${TARGET_NAME}
TARGET = ${OUT_DIR}/${TARGET_NAME}

# C/C++ standard
CFLAGS += -std=c11
CXXFLAGS += -std=c++11

# Defines and flags

# Includes

# Files
OBJS = $(call map_files,${SRC_ROOT},c,${OBJ_ROOT},o) \
    $(call map_files,${SRC_ROOT},cpp,${OBJ_ROOT},o)
DEPS = ${OBJS:.o=.d}

# Library loofah
CPPFLAGS += -I${SRC_ROOT}/..
LDFLAGS += -L${OUT_DIR} -lloofah
LIB_DEPS += ${OUT_DIR}/libloofah.${DL_SUFFIX}

# Library nut
CPPFLAGS += -I${NUT_PATH}/src
LDFLAGS += -L${NUT_PATH}/proj/makefile/${OUT_DIR_NAME} -lnut
LIB_DEPS += ${NUT_PATH}/proj/makefile/${OUT_DIR_NAME}/libnut.${DL_SUFFIX}

# Other libraries
ifeq (${HOST}, Linux)
    LDFLAGS += -lpthread -latomic
endif

# Targets
.PHONY: all
all: ${TARGET}

.PHONY: clean
clean:
	$(RM) ${OBJS} ${DEPS} ${TARGET}

.PHONY: rebuild
rebuild:
	$(MAKE) -f test-loofah.mk clean
	$(MAKE) -f test-loofah.mk all

# Rules
include ${NUT_PATH}/proj/makefile/mkinclude/common_rules.mk
include ${NUT_PATH}/proj/makefile/mkinclude/app_rules.mk
