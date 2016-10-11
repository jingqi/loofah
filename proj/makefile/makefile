#!/user/bin/env make

all:
	make -f test_loofah.mk

clean:
	make -f test_loofah.mk clean
	make -f loofah.mk clean
	(cd ../../lib/nut.git/proj/makefile ; make -f nut.mk clean)

rebuild: clean all

run: all
	make -f test_loofah.mk run

cgdb: all
	make -f test_loofah.mk cgdb

lldb: all
	make -f test_loofah.mk lldb

valgrind: all
	make -f test_loofah.mk valgrind
