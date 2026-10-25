#!/bin/bash
# Build the firmware SNMP parser for the host and run the checks in test_snmp.py.
# Copies SNMP.cpp/SNMP.h next to the stubs so that the stub headers win over the real ones.
set -e
here=$(cd "$(dirname "$0")" && pwd)
work=$(mktemp -d)
trap '[ -n "${NPR_KEEP_WORK:-}" ] && cp -R "$work" "$NPR_KEEP_WORK/$(basename "$here")"; rm -rf "$work"' EXIT   # tools/coverage.sh keeps the work dir
cp "$here"/stubs/*.h "$here"/driver.cpp "$work"/
cp "$here"/../../source/SNMP.cpp "$here"/../../source/SNMP.h "$work"/
c++ -std=gnu++17 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -Wall -o "$work/snmp_host" "$work"/driver.cpp "$work"/SNMP.cpp 2>"$work/build.log" || { cat "$work/build.log"; exit 1; }
SNMP_HOST_BIN="$work/snmp_host" python3 "$here/test_snmp.py" "$@"
