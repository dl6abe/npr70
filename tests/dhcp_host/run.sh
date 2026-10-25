#!/bin/bash
# Differential test of the DHCP server code: builds the frozen original (DHCP_ref.cpp) and the current
# DHCP part of source/DHCP_ARP.cpp for the host, feeds both the same random packets and compares every answer.
set -e
here=$(cd "$(dirname "$0")" && pwd)
work=$(mktemp -d)
trap '[ -n "${NPR_KEEP_WORK:-}" ] && cp -R "$work" "$NPR_KEEP_WORK/$(basename "$here")"; rm -rf "$work"' EXIT   # tools/coverage.sh keeps the work dir
cp "$here"/stubs/*.h "$here"/driver.cpp "$here"/ip_conv.inc "$work"/
python3 - "$here" "$work" <<'PY'
import sys, os
here, work = sys.argv[1:3]
srcpath = os.path.realpath(here + "/../../source/DHCP_ARP.cpp")
src = open(srcpath, newline='').read().replace('\r\n', '\n').split('\n')
a = next(i for i, l in enumerate(src) if l.startswith("#define DHCP_ARP_tab_size"))
b = next(i for i, l in enumerate(src) if l.startswith("void DHCP_ARP_print_entries"))
head = open(here + "/DHCP_ref.cpp").read().split("#define DHCP_ARP_tab_size")[0]
open(work + "/DHCP_new.cpp", "w").write(head + '#line %d "%s"\n' % (a + 1, srcpath) + '\n'.join(src[a:b]) + '\n')
PY
cp "$here"/DHCP_ref.cpp "$work"/
# Zero-init stack variables so that the read of an unset value in the original gives the same result in both builds
flags="-std=gnu++17 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -Wall -ftrivial-auto-var-init=zero -I$work"
c++ $flags -o "$work/ref" "$work/driver.cpp" "$work/DHCP_ref.cpp" 2>"$work/build_ref.log" || { cat "$work/build_ref.log"; exit 1; }
c++ $flags -o "$work/new" "$work/driver.cpp" "$work/DHCP_new.cpp" 2>"$work/build_new.log" || { cat "$work/build_new.log"; exit 1; }
"$work/ref" "$@" > "$work/ref.out"
"$work/new" "$@" > "$work/new.out"
lines=$(wc -l < "$work/ref.out")
if cmp -s "$work/ref.out" "$work/new.out"; then
	echo "DHCP host test OK: $lines identical output lines"
else
	diff "$work/ref.out" "$work/new.out" | head -20
	echo "DHCP host test FAILED"
	exit 1
fi
