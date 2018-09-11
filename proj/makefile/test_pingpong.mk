#!/user/bin/env make

TARGET_NAME = test_pingpong
SRC_ROOT = ../../src/${TARGET_NAME}

# preface rules
include ${NUT_DIR}/proj/makefile/preface_rules.mk

# Includes
CPPFLAGS += -I${SRC_ROOT}/.. -I${NUT_DIR}/src

# Defines
CPPFLAGS +=

# C++ standard
CXXFLAGS += -std=c++11

# LIB
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
	rm -rf ${OBJS}
	rm -rf ${DEPS}
	rm -rf ${TARGET}

rebuild:
	$(MAKE) -f test_pingpong.mk clean
	$(MAKE) -f test_pingpong.mk all

${NUT_DIR}/proj/makefile/${OUT_DIR_NAME}/libnut.${DL_SUFFIX}:
	cd ${NUT_DIR}/proj/makefile ; $(MAKE) -f nut.mk

${OUT_DIR}/libnut.${DL_SUFFIX}: ${NUT_DIR}/proj/makefile/${OUT_DIR_NAME}/libnut.${DL_SUFFIX}
	cp -f $< $@

${OUT_DIR}/libloofah.${DL_SUFFIX}:
	$(MAKE) -f loofah.mk

# rules
include ${NUT_DIR}/proj/makefile/common_rules.mk
include ${NUT_DIR}/proj/makefile/app_rules.mk
