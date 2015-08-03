
all:
	make -f loofah_makefile
	make -f test_loofah_makefile

clean:
	make -f loofah_makefile clean
	make -f test_loofah_makefile clean

rebuild: clean all

run: all
	make -f test_loofah_makefile run

cgdb: all
	make -f test_loofah_makefile cgdb

valgrind: all
	make -f test_loofah_makefile valgrind
