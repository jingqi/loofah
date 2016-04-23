#!/user/bin/env make

DEBUG ?= 0

CC = g++
LD = gcc
AR = ar

# variables
SRC_ROOT = ../../src/loofah
ifeq (${DEBUG}, 1)
	OUT_DIR = $(CURDIR)/debug
	NUT_OUT_DIR = $(CURDIR)/../../lib/nut.git/proj/makefile/debug
else
	OUT_DIR = $(CURDIR)/release
	NUT_OUT_DIR = $(CURDIR)/../../lib/nut.git/proj/makefile/release
endif
OBJ_ROOT = ${OUT_DIR}/obj/loofah
THIS = loofah.mk

# INC
INC = -I../../lib/nut.git/src -I${SRC_ROOT}

# DEF
DEF =

# CC_FLAGS
HOST = $(shell uname -s)
CC_FLAGS = -Wall -fPIC -std=c++11
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
else
	LIB += -lpthread
endif
LIB += -L${NUT_OUT_DIR} -lnut
LIB_DEPS += ${NUT_OUT_DIR}/libnut.a

# OBJS, DEPS
DIRS = $(shell find ${SRC_ROOT} -maxdepth 10 -type d)
CPPS = $(foreach dir,${DIRS},$(wildcard $(dir)/*.cpp))
OBJS = $(patsubst ${SRC_ROOT}%.cpp,${OBJ_ROOT}%.o,${CPPS})
DEPS = ${OBJS:.o=.d}

# TARGET
TARGET = ${OUT_DIR}/libloofah.a

# mkdirs
$(shell mkdir -p $(patsubst ${SRC_ROOT}%,${OBJ_ROOT}%,${DIRS}))

.PHONY: all clean rebuild run

all: others ${TARGET}

others:
	(cd ../../lib/nut.git/proj/makefile ; make -f nut.mk)

clean:
	rm -rf ${OBJS}
	rm -rf ${DEPS}
	rm -rf ${TARGET}
	(cd ../../lib/nut.git/proj/makefile ; make -f nut.mk clean)

rebuild: clean all

run: ${TARGET}
	${TARGET}

${TARGET}: ${OBJS} ${LIB_DEPS} ${THIS}
	rm -f $@
	${AR} cqs $@ ${OBJS}

${OBJ_ROOT}/%.o: ${SRC_ROOT}/%.cpp ${THIS}
	${CC} ${INC} ${DEF} ${CC_FLAGS} -c $< -o $@

## 动态生成依赖关系
# %.d: %.cpp
${OBJ_ROOT}/%.d:${SRC_ROOT}/%.cpp ${THIS}
	@rm -f $@
	@# 向 *.d.$ 中写入 "xx/xx/*.d xx/xx/*.o:\" 这样一个字符串
	@echo '$@ $@.o:\\' | sed 's/[.]d[.]o/.o/g' > $@.$$
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
