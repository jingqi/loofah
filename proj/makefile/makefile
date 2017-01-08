#!/user/bin/env make

.PHONY: all clean rebuild run profile

all:
	$(MAKE) -f test_loofah.mk
	$(MAKE) -f test_pingpong.mk

clean:
	$(MAKE) -f test_loofah.mk clean
	$(MAKE) -f test_pingpong.mk clean

rebuild:
	$(MAKE) clean
	$(MAKE) all

run:
	$(MAKE) -f test_loofah.mk run

profile:
	$(MAKE) -f test_pingpong.mk run
