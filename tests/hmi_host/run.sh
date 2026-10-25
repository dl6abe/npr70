#!/bin/bash
# Differential test of the 'set' command: builds the frozen original (HMI_set_ref.cpp) and the current
# set code of source/HMI_telnet.cpp for the host, runs the same list of commands and compares the output.
set -e
here=$(cd "$(dirname "$0")" && pwd)
work=$(mktemp -d)
trap '[ -n "${NPR_KEEP_WORK:-}" ] && cp -R "$work" "$NPR_KEEP_WORK/$(basename "$here")"; rm -rf "$work"' EXIT   # tools/coverage.sh keeps the work dir
cp "$here"/stubs/*.h "$here"/driver.cpp "$here"/ip_conv.inc "$here"/HMI_set_ref.cpp "$work"/
python3 - "$here" "$work" <<'PY'
import sys, os
here, work = sys.argv[1:3]
srcpath = os.path.realpath(here + "/../../source/HMI_telnet.cpp")
src = open(srcpath, newline='').read().replace('\r\n', '\n').split('\n')
a = next(i for i, l in enumerate(src) if l.startswith("// Handlers of the 'set"))
f = next(i for i, l in enumerate(src) if l.startswith("unsigned char HMI_yes_no_2int"))
b = next(i for i in range(f, len(src)) if src[i] == "}")
head = open(here + "/HMI_set_ref.cpp").read().split("void HMI_set_command")[0]
open(work + "/HMI_set_new.cpp", "w").write(head + '#line %d "%s"\n' % (a + 1, srcpath) + '\n'.join(src[a:b+1]) + '\n')
PY
flags="-std=gnu++17 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -Wall -Wno-format-security -I$work"
c++ $flags -o "$work/ref" "$work/driver.cpp" "$work/HMI_set_ref.cpp" 2>"$work/build_ref.log" || { cat "$work/build_ref.log"; exit 1; }
c++ $flags -o "$work/new" "$work/driver.cpp" "$work/HMI_set_new.cpp" 2>"$work/build_new.log" || { cat "$work/build_new.log"; exit 1; }
"$work/ref" > "$work/ref.out"
"$work/new" > "$work/new.out"
# The new code may add warning lines (network settings that do not fit); the reference does not know them.
python3 - "$work/new.out" <<'PY'
import re, sys
t = open(sys.argv[1], newline="").read()
open(sys.argv[1], "w", newline="").write(re.sub(r"OUT\[warning: [^\]]*\]\n", "", t))
PY
lines=$(wc -l < "$work/ref.out")
if cmp -s "$work/ref.out" "$work/new.out"; then
	echo "HMI set host test OK: $lines identical output lines"
else
	diff "$work/ref.out" "$work/new.out" | head -20
	echo "HMI set host test FAILED"
	exit 1
fi
