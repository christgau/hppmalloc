CC=gcc
CFLAGS=-O0 -g -Wall -Wextra -DDEBUG
LIBRARY_NAME=libhppalloc.so

.PHONY: all

all: $(LIBRARY_NAME)

$(LIBRARY_NAME): hppalloc.c
	$(CC) $(CFLAGS) -shared -fPIC $^ -o $@

.PHONY: test

test: $(LIBRARY_NAME)
	make -C test

.PHONY: clean

clean:
	make -C test clean
	rm -f *.o $(LIBRARY_NAME)
