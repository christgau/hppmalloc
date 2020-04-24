F90=gfortran

CFLAGS=-O0 -g -lhppalloc -L.. -Wall -Wextra -pedantic -Wl,--rpath=.. -I.. -std=c99
FFLAGS=-O0 -g -lhppalloc -L.. -Wall -Wextra -pedantic -Wl,--rpath=.. -I.. -fcheck=all -fbacktrace -fdump-fortran-optimized -cpp

TESTS=simple huge toohuge alloc_seq stress

.PHONY: test

test: $(TESTS)
	@for t in ./test-*.sh; do if $$t >/dev/null 2>&1; then echo "success $$t"; else echo "failed $$t"; fi; done

%: %.f90
	$(F90) $(FFLAGS) $^ -o $@

clean:
	rm -f *.o $(TESTS)
