#!/user/bin/env make

TARGET_NAME = test_loofah
SRC_ROOT = ../../src/${TARGET_NAME}

# preface rules
include ${NUT_DIR}/proj/makefile/preface_rules.mk

# INC
INC += -I${SRC_ROOT}/.. -I${NUT_DIR}/src

# DEF
DEF +=

# CXX_FLAGS
CXX_FLAGS += -std=c++11

# LIB
ifeq (${HOST}, Linux)
	LIB += -lpthread
endif
LIB += -L${OUT_DIR} -lnut -lloofah
LIB_DEPS += ${OUT_DIR}/libnut.${DL_SUFFIX} \
			${OUT_DIR}/libloofah.${DL_SUFFIX}

# LD_FLAGS
LD_FLAGS +=

# TARGET
TARGET = ${OUT_DIR}/${TARGET_NAME}

.PHONY: all clean rebuild

all: ${TARGET}

clean:
	rm -rf ${OBJS}
	rm -rf ${DEPS}
	rm -rf ${TARGET}

rebuild:
	$(MAKE) -f test_loofah.mk clean
	$(MAKE) -f test_loofah.mk all

${OUT_DIR}/libnut.${DL_SUFFIX}:
	cd ${NUT_DIR}/proj/makefile ; $(MAKE) -f nut.mk
	cp -f ${NUT_DIR}/proj/makefile/${OUT_DIR_NAME}/libnut.${DL_SUFFIX} $@

${OUT_DIR}/libloofah.${DL_SUFFIX}:
	$(MAKE) -f loofah.mk

# rules
include ${NUT_DIR}/proj/makefile/common_rules.mk
include ${NUT_DIR}/proj/makefile/app_rules.mk
