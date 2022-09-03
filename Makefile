TARGET=stypist

include config.mk

stypist: stypist.c config.h
	$(CC) $@.c $(CFLAGS) -o $@

config.h: config.def.h
	cp config.def.h $@

clean:
	rm stypist config.h

test: stypist
	cat stypist.c | ./stypist

all: stypist
