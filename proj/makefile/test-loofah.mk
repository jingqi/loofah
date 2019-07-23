#!/user/bin/env make

TARGET_NAME = test-loofah
SRC_ROOT = ../../src/${TARGET_NAME}

# Preface rules
include ${NUT_PATH}/proj/makefile/preface_rules.mk

# Includes
CPPFLAGS += -I${SRC_ROOT}/.. -I${NUT_PATH}/src

# Defines
CPPFLAGS +=

# C/C++ standard
CFLAGS += -std=c11
CXXFLAGS += -std=c++11

# Libraries
ifeq (${HOST}, Linux)
	LDFLAGS += -lpthread
endif
LDFLAGS += -L${OUT_DIR} -lnut -lloofah
LIB_DEPS += ${OUT_DIR}/libnut.${DL_SUFFIX} \
			${OUT_DIR}/libloofah.${DL_SUFFIX}

# TARGET
TARGET = ${OUT_DIR}/${TARGET_NAME}

.PHONY: all clean rebuild

all: ${TARGET}

clean:
	rm -rf ${OBJS} ${DEPS} ${TARGET}

rebuild:
	$(MAKE) -f test-loofah.mk clean
	$(MAKE) -f test-loofah.mk all

${NUT_PATH}/proj/makefile/${OUT_DIR_NAME}/libnut.${DL_SUFFIX}:
	cd ${NUT_PATH}/proj/makefile ; $(MAKE) -f nut.mk

${OUT_DIR}/libnut.${DL_SUFFIX}: ${NUT_PATH}/proj/makefile/${OUT_DIR_NAME}/libnut.${DL_SUFFIX}
	cp -f $< $@

${OUT_DIR}/libloofah.${DL_SUFFIX}:
	$(MAKE) -f loofah.mk

# Rules
include ${NUT_PATH}/proj/makefile/common_rules.mk
include ${NUT_PATH}/proj/makefile/app_rules.mk
