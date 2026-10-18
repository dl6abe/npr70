#!/bin/bash
# Host test of the helpers of the diagnosis commands (source/diag_core.h): statistics of the latency probe and the
# evaluation of a stack that was filled with a pattern (issues #1 and #10). Run it after each change of diag_core.h.
set -e
here=$(cd "$(dirname "$0")" && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
c++ -std=gnu++11 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -Wall -I"$here/../../source" \
    -o "$work/diag" "$here/driver.cpp" 2>"$work/build.log" || { cat "$work/build.log"; exit 1; }
"$work/diag" && echo "diag host test OK" || { echo "diag host test FAILED"; exit 1; }
