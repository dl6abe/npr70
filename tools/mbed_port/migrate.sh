#!/bin/bash
# Move the project to another mbed-os tag: replace npr70/mbed-os by the trimmed tree of the tag and
# regenerate the mbed-os part of npr70/Makefile and npr70/mbed_config.h (export_mbed.sh must have run for the
# tag). Nothing is committed or flashed. The old state is in git (git checkout -- npr70/mbed-os ...).
#
# usage: ./migrate.sh TAG [WORK]      env HAL_TICK=1 (default, see source/hal_tick_wrap.cpp), HAL_TICK= to leave it out
set -e
tag=${1:?usage: migrate.sh TAG [WORK]}
work=${2:-$HOME/.cache/npr70-mbed}
here=$(cd "$(dirname "$0")" && pwd)
npr70=$(cd "$here/../.." && pwd)
ref="mbed-os-$tag"

# 1. trimmed tree: the directories the build uses, STM32L4 only (variants L432xC, L475xG, L476xG, L486xG)
new=$(mktemp -d)
git -C "$work/mbed-git" archive "$ref" mbed.h cmsis drivers events hal platform rtos targets/targets.json \
    targets/TARGET_STM tools | tar -x -C "$new"
(cd "$new/targets/TARGET_STM" && for d in TARGET_STM32*; do [ "$d" = TARGET_STM32L4 ] || rm -rf "$d"; done)
(cd "$new/targets/TARGET_STM/TARGET_STM32L4" && for d in TARGET_STM32L*; do
    case $d in TARGET_STM32L432xC|TARGET_STM32L475xG|TARGET_STM32L476xG|TARGET_STM32L486xG) ;; *) rm -rf "$d";; esac
done)
find "$new" -name '*.pyc' -delete

# 2. Makefile and mbed_config.h from a build of the project with this tag (build_port.sh adds the switches)
HAL_TICK=${HAL_TICK-1} BUILD_NAME="migrate-$tag" EXTRA_CFLAGS=${EXTRA_CFLAGS:--Wno-error=implicit-function-declaration} \
    "$here/build_port.sh" "$tag" "$work"
built="$work/build-migrate-$tag"

# 3. swap in (CRLF as the project files have it)
rm -rf "$npr70/mbed-os"
mv "$new" "$npr70/mbed-os"
python3 - "$built" "$npr70" <<'PY'
import sys
built, npr70 = sys.argv[1:3]
for name in ("Makefile", "mbed_config.h"):
    b = open("%s/%s" % (built, name), "rb").read().replace(b"\r\n", b"\n").replace(b"\n", b"\r\n")
    open("%s/%s" % (npr70, name), "wb").write(b)
PY
echo "migrated to $tag: build in $npr70 with make -j8"
