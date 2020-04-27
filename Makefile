CFLAGS=-O0 -g -Wall -Wextra -DDEBUG -std=c99
F90=gfortran
FFLAGS=-fsyntax-only -Wall -Wextra
LDFLAGS=-lbsd
LIBRARY_NAME=libhppalloc.so
HOOK_LIBRARY_NAME=libhppahook.so

.PHONY: all

all: $(LIBRARY_NAME) $(HOOK_LIBRARY_NAME) hppalloc.mod

$(LIBRARY_NAME): hppalloc.c
	$(CC) $(CFLAGS) $(LDFLAGS) -shared -fPIC $^ -o $@

$(HOOK_LIBRARY_NAME): hppahook.c hppalloc.c
	$(CC) $(CFLAGS) $(LDFLAGS) -shared -fPIC -ldl -DHPPA_EXTERN_MALLOC $^ -o $@

hppalloc.mod: hppalloc.f90
	$(F90) $(FFLAGS) $^

.PHONY: test

test: $(LIBRARY_NAME)
	make -C test

.PHONY: clean

clean:
	make -C test clean
	rm -f *.o $(LIBRARY_NAME) $(HOOK_LIBRARY_NAME)
