#!/user/bin/env make

# 3rdparty
export NUT_PATH=$(CURDIR)/../../lib/nut.git

# Preface
include ${NUT_PATH}/proj/makefile/mkinclude/vars.mk

# Targets
.PHONY: all
all: loofah test-loofah test-pingpong

.PHONY: nut
nut:
	cd ${NUT_PATH}/proj/makefile ; $(MAKE) -f nut.mk
	mkdir -p ${OUT_DIR}
	cp -f ${NUT_PATH}/proj/makefile/${OUT_DIR_NAME}/libnut.${DL_SUFFIX} ${OUT_DIR}/libnut.${DL_SUFFIX}

.PHONY: loofah
loofah: nut
	$(MAKE) -f loofah.mk

.PHONY: test-loofah
test-loofah: nut loofah
	$(MAKE) -f test-loofah.mk

.PHONY: test-pingpong
test-pingpong: nut loofah
	$(MAKE) -f test-pingpong.mk

.PHONY: clean
clean:
	cd ${NUT_PATH}/proj/makefile ; $(MAKE) -f nut.mk clean
	$(RM) -r ${OUT_DIR_NAME}

.PHONY: rebuild
rebuild:
	$(MAKE) clean
	$(MAKE) all

.PHONY: run
run: test-loofah
	$(MAKE) -f test-loofah.mk run

.PHONY: run-pingpong
run-pingpong: test-pingpong
	$(MAKE) -f test-pingpong.mk run

.PHONY: valgrind
valgrind: test-loofah
	$(MAKE) -f test-loofah.mk valgrind
