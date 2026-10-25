#!/bin/bash
# Host test of the IP address checks of the console (issue #6): the real functions
# HMI_parse_IP, HMI_netmask_valid, HMI_host_IP_valid, HMI_net_problems, the 'set' handlers for the IP values and
# the 'save' check are cut out of source/HMI_telnet.cpp and run under ASan/UBSan with a table of cases.
# Run it after each change of these functions. The differential test tests/hmi_host/run.sh covers the unchanged
# commands; this one covers what the new rules change.
set -e
here=$(cd "$(dirname "$0")" && pwd)
work=$(mktemp -d)
trap '[ -n "${NPR_KEEP_WORK:-}" ] && cp -R "$work" "$NPR_KEEP_WORK/$(basename "$here")"; rm -rf "$work"' EXIT   # tools/coverage.sh keeps the work dir
cp "$here"/../hmi_host/stubs/*.h "$here"/driver.cpp "$work"/
python3 - "$here" "$work" <<'PY'
import sys, os
here, work = sys.argv[1:3]
srcpath = os.path.realpath(here + "/../../source/HMI_telnet.cpp")
src = open(srcpath, newline='').read().replace('\r\n', '\n').split('\n')
a = next(i for i, l in enumerate(src) if l.startswith("// Handlers of the 'set"))
f = next(i for i, l in enumerate(src) if l.startswith("unsigned char HMI_yes_no_2int"))
b = next(i for i in range(f, len(src)) if src[i] == "}")
s = next(i for i, l in enumerate(src) if l.startswith("static void HMI_save_command(void) {"))
e = next(i for i in range(s, len(src)) if src[i] == "}")
open(work + "/ip_part.inc", "w").write('#line %d "%s"\n' % (a + 1, srcpath) + '\n'.join(src[a:b+1]) + '\n' + '#line %d "%s"\n' % (s + 1, srcpath) + '\n'.join(src[s:e+1]) + '\n')
PY
flags="-std=gnu++17 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -Wall -Wno-format-security -Wno-unused-function -I$work"
c++ $flags -o "$work/ip" "$work/driver.cpp" 2>"$work/build.log" || { cat "$work/build.log"; exit 1; }
"$work/ip" && echo "ip host test OK" || { echo "ip host test FAILED"; exit 1; }
