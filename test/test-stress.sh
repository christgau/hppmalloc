#!/bin/sh

# do not use malloc, only anonymous (hugepage-backed)
export HPPA_INITIAL_STRATEGY=2

# allow small allocations
export HPPA_ALLOC_THRESHOLD=524288
./stress 48 || exit 2

# use file-backed mapping, create file at system's default tmp location
tmp_dir="$(mktemp -d)"

function clean()
{
	test -d "$tmp_dir" && find "$tmp_dir" -delete
}

trap clean EXIT

# set strategy to file-based-only
export HPPA_INITIAL_STRATEGY=4
export HPPA_BASE_PATH="$tmp_dir"
./stress 48 || exit 4
