#!/user/bin/env make

DEBUG ?= 0

TARGET_NAME = loofah

CC = g++
LD = gcc
AR = ar

# variables
SRC_ROOT = ../../src/${TARGET_NAME}
ifeq (${DEBUG}, 1)
	OUT_DIR = $(CURDIR)/debug
	NUT_OUT_DIR = $(CURDIR)/../../lib/nut.git/proj/makefile/debug
else
	OUT_DIR = $(CURDIR)/release
	NUT_OUT_DIR = $(CURDIR)/../../lib/nut.git/proj/makefile/release
endif
OBJ_ROOT = ${OUT_DIR}/obj/${TARGET_NAME}

# INC
INC += -I../../lib/nut.git/src -I${SRC_ROOT}/..

# DEF
DEF += -DBUILD_LOOFAH_DLL

# CC_FLAGS
HOST = $(shell uname -s)
CC_FLAGS += -Wall -fPIC -std=c++11
ifeq (${DEBUG}, 1)
	CC_FLAGS += -DDEBUG -g
else
	CC_FLAGS += -DNDEBUG -O2
endif
ifeq (${HOST}, Darwin)
	CC_FLAGS += -stdlib=libc++
else
	CC_FLAGS += -rdynamic
endif

# LIB
ifeq (${HOST}, Darwin)
	LIB += -lc++
	LIB_DEPS += ${NUT_OUT_DIR}/libnut.dylib
else
	LIB += -lpthread
	LIB_DEPS += ${NUT_OUT_DIR}/libnut.so
endif
LIB += -L${NUT_OUT_DIR} -lnut

# LD_FLAGS
ifeq (${HOST}, Darwin)
	# 相当于执行 `install_name_tool -id @rpath/lib${TARGET_NAME}.dylib ${TARGET}`
	LD_FLAGS += -install_name @rpath/lib${TARGET_NAME}.dylib
endif

# OBJS, DEPS
DIRS = $(shell find ${SRC_ROOT} -maxdepth 10 -type d)
CPPS = $(foreach dir,${DIRS},$(wildcard $(dir)/*.cpp))
OBJS = $(patsubst ${SRC_ROOT}%.cpp,${OBJ_ROOT}%.o,${CPPS})
DEPS = ${OBJS:.o=.d}

# TARGET
ifeq (${HOST}, Darwin)
	TARGET = ${OUT_DIR}/lib${TARGET_NAME}.dylib
else
	TARGET = ${OUT_DIR}/lib${TARGET_NAME}.so
endif

# mkdirs
$(shell mkdir -p $(patsubst ${SRC_ROOT}%,${OBJ_ROOT}%,${DIRS}))

.PHONY: all others clean rebuild run

all: others ${TARGET}

others:
	(cd ../../lib/nut.git/proj/makefile ; make -f nut.mk)

clean:
	rm -rf ${OBJS}
	rm -rf ${DEPS}
	rm -rf ${TARGET}

rebuild: clean all

run: ${TARGET}
	${TARGET}

THIS_MAKEFILE = $(abspath $(firstword $(MAKEFILE_LIST)))
${TARGET}: ${OBJS} ${LIB_DEPS} ${THIS_MAKEFILE}
	${LD} ${OBJS} ${LIB} ${LD_FLAGS} -shared -o $@

${OBJ_ROOT}/%.o: ${SRC_ROOT}/%.cpp ${THIS_MAKEFILE}
	${CC} ${INC} ${DEF} ${CC_FLAGS} -c $< -o $@

## 动态生成依赖关系
# %.d: %.cpp
${OBJ_ROOT}/%.d: ${SRC_ROOT}/%.cpp ${THIS_MAKEFILE}
	@rm -f $@
	@# 向 *.d.$ 中写入 "xx/xx/*.d xx/xx/*.o:\" 这样一个字符串
	@echo '$@ $@.o:\' | sed 's/[.]d[.]o/.o/g' > $@.$$
	@# 向 *.d.$$ 中写入用 gcc -MM 生成的初始依赖关系
	${CC} ${INC} ${DEF} ${CC_FLAGS} -MM $< > $@.$$.$$
	@# 将 *.d.$$ 中内容去除冒号前的内容，剩余内容写入 *.d.$ 中
	@sed 's/^.*[:]//g' < $@.$$.$$ >> $@.$$
	@# 空行
	@echo '' >> $@.$$
	@# 对 *.d.$$ 的内容依此处理：
	@#	sed 去除冒号前的内容
	@#	sed 去除续行符
	@#	fmt 每个连续单词作为单独一行
	@#	sed 去除行首空白
	@#	sed 行尾添加冒号
	@sed -e 's/.*://' -e 's/\\$$//' < $@.$$.$$ | fmt -1 | \
		sed -e 's/^ *//' -e 's/$$/:/' >> $@.$$
	@# 最后清理
	@rm -f $@.$$.$$
	@mv $@.$$ $@

# 引入动态依赖关系
#	起首的'-'符号表示忽略错误命令(这里忽略不存在的文件，不再打warning)
-include ${DEPS}
