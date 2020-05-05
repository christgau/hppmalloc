#!/bin/sh

# check if there are two allocated and one free block just before frees are issued

# heap printing includes a trailer and header, so three lines for 
# blocks + 2 lines = 5 before free indicator
export HPPA_PRINT_HEAP=1
export HPPA_LOGLEVEL=debug
buffer="$(./huge | fgrep -B5 '+++free' | fgrep -A3 -- '--- anon' | sed 1d)"

used_count="$(fgrep -c 'used: 1' <<< "$buffer")"
free_count="$(fgrep -c 'used: 0' <<< "$buffer")"

test $used_count -eq 2 || exit 1
test $free_count -eq 1 || exit 1
