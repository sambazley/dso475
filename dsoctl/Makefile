CFLAGS+=-Wall -Wextra -Og -ggdb
CFLAGS+=-I..

CFLAGS+=$(shell pkgconf --cflags libusb-1.0)
LDLIBS+=$(shell pkgconf --libs libusb-1.0)

dsoctl: dsoctl.c

clean:
	rm -f *.o dsoctl

PHONY: clean
