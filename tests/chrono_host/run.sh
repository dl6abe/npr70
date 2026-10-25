#!/bin/bash
# Table test of the conversion of Timer::read_us() (int, 32 bit) to Timer::elapsed_time().count() (64 bit), issue #16.
# Shows where the cast static_cast<int>() is needed and where it makes no difference. Needs only the C++ compiler.
set -e
here=$(cd "$(dirname "$0")" && pwd)
work=$(mktemp -d)
trap '[ -n "${NPR_KEEP_WORK:-}" ] && cp -R "$work" "$NPR_KEEP_WORK/$(basename "$here")"; rm -rf "$work"' EXIT   # tools/coverage.sh keeps the work dir
c++ -std=gnu++17 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -Wall -o "$work/chrono_host" "$here/driver.cpp"
"$work/chrono_host"
