#!/bin/sh

# 2GB for the heap
export HPPA_SIZE_ANON=$((2 * 1024 * 1024 * 1024))
export HPPA_PRINT_HEAP=1

for bin in simple huge alloc_seq; do
	last_heap=$(./$bin | fgrep '[hpp] total' | tail -n1 | cut -f2 -d:)
	heap_size=$(cut -f2 -d, <<< "$last_heap" | awk '{ print $1 }')
	blocks=$(awk '{ print $1 }' <<< "$last_heap")
	test $heap_size -eq $HPPA_SIZE_ANON || exit 1
	test $blocks -eq 1 || exit 1
done
