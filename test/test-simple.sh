#!/bin/sh

./simple | fgrep -q 'failure' || exit 0
