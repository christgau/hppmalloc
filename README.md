# hppalloc - huge page persistent memory allocator

The library provides a memory allocator which relies on huge pages and may
use persistent memory exposed in _App Direct_ mode. It can be used either
directly with dedicated library calls or with a shared library that replaces
`malloc`(3) calls from the applications or language runtime with allocations
from the library.

## Background

Applications with large memory footprints may suffer from a high number of page
faults as well as TLB misses.  Although operating systems, like Linux, provide
mechanisms like Transparent Huge Pages (THP), they may either not be able to
provide huge pages on demand due to memory fragmentation or they consume
additional time to defragment the memory to hand out huge pages afterwards.
The allocator presented here uses huge pages reserved for the system before the
application starts. They are `mmap`d into the address space and serve as
underlying, non-persistent, and anonymous memory range.

Further, an application may decide on which level in the memory hierarchy it
wants data to be stored. In case of storage class (persistent) memory (SCM) this
is possible in the so called `App Direct Mode` which exposes the persistent
memory as file system. The library `mmap`s a file on such a device (or any other
path in the file system) with a requested size and provides memory blocks from
this mapping if asked for.

To be convenient for the user, this library provides a call similar to
`malloc`/`calloc` for memory allocation. The library can be configured
dynamically to choose the method used for the allocation.

By doing so, both uncontrollable references to persistent memory in `Memory Mode`
as well as frequent page faults/TLB misses can be avoided.

## Usage 

In principle, the library can be used in two variants: either using its
dedicated interface or using it (almost) transparently for legacy applications.

### Dedicated Interface

The library offers two memory allocation functions

1. `void* hpp_alloc(size_t count, size_t elem_size)`
   This call, with a `calloc`-like signature, returns a pointer to the allocated
   memory or `NULL` if the allocation failed

2. `void hpp_free(void* ptr)`
   The counterpart for hppalloc. Removes the allocation from memory. It is safe
   to call this function with `NULL` as argument.

3. `void hpp_set_mode(int mode)`
   Set the mode used internally for the memory allocation. The mode steers how
   `hpp_alloc` behaves. This mode can be a bitwise OR-combination of the
   following values

   * `HPPA_AS_MALLOC` - select `malloc` as allocation strategy. If active,
     `hpp_alloc` will try to call libc's `malloc` to satisfy the allocation
     request if every other strategy has failed.
   * `HPPA_AS_ANON` - memory allocation requests are tried to be satisfied from
     the huge page memory mapping.
   * `HPPA_AS_NAMED` or `HPPA_AS_PMEM` - same as `HPPA_AS_ANON` but instead of
     the huge page mapping, the named/file mapping is used to satisfy the
     allocation. This mode effectively disables the `HPPA_AS_ANON` mode, even
     when both are specified (this may be subject to change).

For all these function, a Fortran (2003+) binding exists.

In order allocate memory from the huge page mapping and use malloc as fallback,
the following calls would be required

```
hpp_set_mode(HPPA_AS_MALLOC | HPPA_AS_ANON);
/* allocate four 1 GB chunks */
buf = hpp_alloc(4, 1 << 30);
```

### Transparent Usage

To avoid massive rewrites of existing applications, a library is provided that
can be used to replace existing `malloc` calls. The libhppahook.so library can
be `LD_PRELOAD`ed to do so. Allocations larger than a specified threshold are
passed to the internal allocation mechanism, while smaller ones are handed over
to the original libc-implementation.

See below for details on how to control the inner workings of the library.

## Controlling the operations

Environment variables can be used to control options and internals of the
library and allocator.

* `HPPA_BASE_PATH` - path to an existing directory in the filesystem that will
  hold the files used for file-based mappings. This is intended to point to a
  directory on a persistent memory device (like `/mnt/pmem0`). However, it can
  point to any other path as well.
* `HPPA_ALLOC_THRESHOLD` - only allocations equal or larger than this value will
  be passed to internal mmap-based allocator. Allocations smaller than this
  value are passed to malloc, if enabled in the library mode (see above).
* `HPPA_SIZE_ANON` and `HPPA_SIZE_NAMED` - specify the size of the mmap-pools
  used by the library/process. For the anonymous/huge page pool, an according
  number of huge pages should have been allocated before-hand by the operating
  system. For the named pool, the targeted device needs to have sufficient space
  left to hold the created files. Note that the files are not listed by `ls`.
* `HPPA_LOGLEVEL` - If the library is compiled with debug support (DEBUG
  symbol), a log level from the following can be chosen: EMERG, ALERT, CRIT,
  ERR, WARN, NOTE, INFO, DEBUG.
* `HPPA_PRINT_HEAP` - if the log level is set to DEBUG and this variable is set
  to 1 or true, then the internal state of the used heap is printed after each
  successful allocation and free operation.

## Usage with Parallel Environments and Performance Tools

When the library should be used in transparent operation mode, i.e. with
`LD_PRELOAD`, care should be taken to affect the desired processes only.
For example, with MPI, the process launcher should not be affected preloading
the library. Thus, the `LD_PRELOAD` variable should only be set _by_ the
launcher, but not _for_ it. Thus, using it with MPI should look this:

 * MPICH: `mpiexec -n 4 -env LD_PRELOAD /path/to/libhppahook.so ./mybin`
 * Open MPI: `mpiexec -n 4 -x LD_PRELOAD=/path/to/libhppahook.so' ./mybin`

In case performance metrics should be obtained from the binary with the tools,
like perf, the performance tool should also be unaffected. In the parallel (MPI)
case it is also undesirable to have memory/huge pages reserved but unused for
every started instance of the performance tool. Using `env` the problem can be
avoided:

 `mpiexec -n 4 perf stat env LD_PRELOAD=/path/to/libhppahook.so ./mybin`

Note: You probably want a wrapper script for perf to write results in different
files.
