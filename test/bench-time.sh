#!/bin/sh

exec_prefix="numactl --physcpubind=0"

# use half the amount of huge pages and hope they are distributed among sockets equally
export HPPA_SIZE_ANON=$(($(cat /sys/kernel/mm/hugepages/hugepages-1048576kB/free_hugepages)/2*1024*1024*1024))
export HPPA_ALLOC_THRESHOLD=$((512 * 1024))

# first run: force usage of anon mmap pool
time_hp="$(HPPA_INITIAL_STRATEGY=2 $exec_prefix /usr/bin/time -f "%e\t%S\t%U" ./stress 2>&1 > /dev/null | cut -f1 -d:)"
echo "times (huge pages: t/k/u): $time_hp"

# second run: force malloc in hpp_alloc
time_malloc="$(HPPA_INITIAL_STRATEGY=1 $exec_prefix /usr/bin/time -f "%e\t%S\t%U" ./stress 2>&1 > /dev/null | cut -f1 -d:)"
echo "times (malloc, t/k/u):     $time_malloc"
