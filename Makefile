TARGET=stypist

# CFLAGS="-g"

build: stypist.c config.h
	$(CC) *.c $(CFLAGS) -o $(TARGET)

config.h: config.def.h
	cp config.def.h config.h

clean:
	rm -rf $(TARGET_DIR)

test: build
	cat stypist.c | ./stypist

all: build
