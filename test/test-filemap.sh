#!/bin/sh

# test if the file-based mapping can be established

export HPPA_BASE_PATH="$(mktemp -d)"
export HPPA_INITIAL_STRATEGY=4

./alloc_seq 

exit_code=$?

rm -rf "$HPPA_BASE_PATH"

exit $exit_code
