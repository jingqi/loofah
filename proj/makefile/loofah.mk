#!/usr/bin/env make

# Preface
include ${NUT_PATH}/proj/makefile/mkinclude/vars.mk
include ${NUT_PATH}/proj/makefile/mkinclude/funcs.mk

# Project vars
TARGET_NAME = loofah
SRC_ROOT = ../../src/${TARGET_NAME}
OBJ_ROOT = ${OUT_DIR}/obj/${TARGET_NAME}
TARGET = ${OUT_DIR}/lib${TARGET_NAME}.${DL_SUFFIX}

# C/C++ standard
CFLAGS += -std=c11
CXXFLAGS += -std=c++11

# Defines and flags
CPPFLAGS += -DBUILDING_LOOFAH

# Includes
CPPFLAGS += -I${SRC_ROOT}/..

# Files
OBJS = $(call map_files,${SRC_ROOT},c,${OBJ_ROOT},o) \
    $(call map_files,${SRC_ROOT},cpp,${OBJ_ROOT},o)
DEPS = ${OBJS:.o=.d}

# Library nut
CPPFLAGS += -I${NUT_PATH}/src
LDFLAGS += -L${NUT_PATH}/proj/makefile/${OUT_DIR_NAME} -lnut
LIB_DEPS += ${NUT_PATH}/proj/makefile/${OUT_DIR_NAME}/libnut.${DL_SUFFIX}

# Other libraries
ifeq (${HOST}, Linux)
    LDFLAGS += -lpthread
endif

# Targets
.PHONY: all
all: ${TARGET}

.PHONY: clean
clean:
	$(RM) ${OBJS} ${DEPS} ${TARGET}

.PHONY: rebuild
rebuild:
	$(MAKE) -f loofah.mk clean
	$(MAKE) -f loofah.mk all

# Rules
include ${NUT_PATH}/proj/makefile/mkinclude/common_rules.mk
include ${NUT_PATH}/proj/makefile/mkinclude/shared_lib_rules.mk
