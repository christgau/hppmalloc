#!/bin/sh

# relaxed heap size
export HPPA_SIZE_ANON=$((1024*1024*1024))
export HPPA_LOGLEVEL=debug

lib_dir=$(realpath $PWD/..)
buffer="$(LD_PRELOAD="$lib_dir/libhppahook.so" ./shadow)"
retval=$?

echo "$buffer"
test $retval -eq 0 || exit $retval

num_allocs=$(fgrep -c 'allocated 1 *' <<< "$buffer")
num_frees=$(fgrep -c 'free block' <<< "$buffer")

test $num_allocs -eq 2 -a $num_allocs -eq 2 || exit 7
