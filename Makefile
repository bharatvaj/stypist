TARGET=stypist

build:
	$(CC) *.c -o $(TARGET)

clean:
	rm -rf $(TARGET_DIR)

all: build
