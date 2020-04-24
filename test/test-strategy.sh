#!/bin/sh

# for file-backed mappings
tmp_dir="$(mktemp -d)"

function clean
{
	test -d "$tmp_dir" && find "$tmp_dir" -delete
}

trap clean EXIT

export HPPA_BASE_PATH="$tmp_dir"

for HPPA_INITIAL_STRATEGY in 1 2 3 4 7; do
	mkdir -p "$tmp_dir"
	export HPPA_INITIAL_STRATEGY
	./alloc_seq || exit $HPPA_INITIAL_STRATEGY
	clean
done
