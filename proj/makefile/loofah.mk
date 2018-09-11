#!/user/bin/env make

TARGET_NAME = loofah
SRC_ROOT = ../../src/${TARGET_NAME}

# preface rules
include ${NUT_DIR}/proj/makefile/preface_rules.mk

# Includes
CPPFLAGS += -I${SRC_ROOT}/.. -I${NUT_DIR}/src

# Defines
CPPFLAGS += -DBUILD_LOOFAH_DLL

# C++ standard
CXXFLAGS += -std=c++11

# LIB LIB_DEPS
ifeq (${HOST}, Linux)
	LDFLAGS += -lpthread
endif
LDFLAGS += -L${OUT_DIR} -lnut
LIB_DEPS += ${OUT_DIR}/libnut.${DL_SUFFIX}

# TARGET
TARGET = ${OUT_DIR}/lib${TARGET_NAME}.${DL_SUFFIX}

.PHONY: all clean rebuild

all: ${TARGET}

clean:
	rm -rf ${OBJS}
	rm -rf ${DEPS}
	rm -rf ${TARGET}

rebuild:
	$(MAKE) -f loofah.mk clean
	$(MAKE) -f loofah.mk all

${NUT_DIR}/proj/makefile/${OUT_DIR_NAME}/libnut.${DL_SUFFIX}:
	cd ${NUT_DIR}/proj/makefile ; $(MAKE) -f nut.mk

${OUT_DIR}/libnut.${DL_SUFFIX}: ${NUT_DIR}/proj/makefile/${OUT_DIR_NAME}/libnut.${DL_SUFFIX}
	cp -f $< $@

# rules
include ${NUT_DIR}/proj/makefile/common_rules.mk
include ${NUT_DIR}/proj/makefile/shared_lib_rules.mk
