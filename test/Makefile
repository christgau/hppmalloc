F90=gfortran

LDFLAGS=-lhppalloc -L..
CFLAGS=-O0 -g -Wall -Wextra -pedantic -Wl,--rpath=.. -I.. -std=c99 $(LDFLAGS)
FFLAGS=-O0 -g -Wall -Wextra -pedantic -Wl,--rpath=.. -I.. -fcheck=all -fbacktrace -fdump-fortran-optimized -cpp

TESTS=simple huge toohuge alloc_seq stress hook shadow

.PHONY: test

test: $(TESTS)
	@for t in ./test-*.sh; do if $$t >/dev/null 2>&1; then echo "success $$t"; else echo "failed $$t"; fi; done

hook: hook.f90
	$(F90) $(FFLAGS) $^ -o $@

%: %.f90
	$(F90) $(FFLAGS) $(LDFLAGS) $^ -o $@

clean:
	rm -f *.o $(TESTS)
