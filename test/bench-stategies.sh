#!/bin/sh

# simple (verbose) benchmarking of the different allocation
# strategies using the stress test binary

# 32 GB
export HPPA_SIZE_ANON=$((32 * 1024 * 1024 * 1024))

# no file-based mapping
export HPPA_SIZE_NAMED=0

# do not drop too small blocks in the benchmark
export HPPA_ALLOC_THRESHOLD=$((512 * 1024))

for strategy in 1 2 3; do
	export HPPA_INITIAL_STRATEGY=$strategy
	perf stat -d -d -d numactl --physcpubind=0 ./stress
done
