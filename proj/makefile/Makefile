#!/user/bin/env make

.PHONY: all clean rebuild run run_pingpong valgrind

export NUT_PATH=$(CURDIR)/../../lib/nut.git

all:
	cd ${NUT_PATH}/proj/makefile ; $(MAKE) -f nut.mk
	$(MAKE) -f loofah.mk
	$(MAKE) -f test-loofah.mk
	$(MAKE) -f test-pingpong.mk

clean:
	cd ${NUT_PATH}/proj/makefile ; $(MAKE) -f nut.mk clean
	$(MAKE) -f loofah.mk clean
	$(MAKE) -f test-loofah.mk clean
	$(MAKE) -f test-pingpong.mk clean

rebuild:
	$(MAKE) clean
	$(MAKE) all

run:
	$(MAKE) -f test-loofah.mk run

run-pingpong:
	$(MAKE) -f test-pingpong.mk run

valgrind:
	$(MAKE) -f test-loofah.mk valgrind
