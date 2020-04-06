F90=gfortran

CFLAGS=-O0 -g -lhppalloc -L.. -Wall -Wextra -pedantic -Wl,--rpath=.. -I.. -std=c99
FFLAGS=-O0 -g -lhppalloc -L.. -Wall -Wextra -pedantic -Wl,--rpath=.. -fcheck=all -fbacktrace -fdump-fortran-optimized -cpp

TESTS=simple huge toohuge

.PHONY: test

test: $(TESTS)
	@for t in $(TESTS); do ./$$t; done

%: %.f90
	$(F90) $(FFLAGS) $^ -o $@

clean:
	rm -f *.o $(TESTS)
