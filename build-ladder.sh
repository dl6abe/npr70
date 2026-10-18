#!/bin/bash
# Build one rung of the compiler flag ladder, starting from the unmodified Makefile.
#
# Flags are cumulative:
#   L0  none (original flags)
#   L1  + -fno-strict-aliasing
#   L2  + -mno-unaligned-access
#   L3  + -fwrapv
#   L4  + -ftrivial-auto-var-init=zero
#   L5  + -fno-aggressive-loop-optimizations
#
# The C++ standard is a separate axis. Default gnu++11, which is what the fork Makefile
# effectively uses (CXX_FLAGS line comes last). Also gnu++98 or gnu++14.
#
# usage: ./build-ladder.sh LEVEL OUTDIR [STD]
#   e.g. ./build-ladder.sh 0 out            original flags
#        ./build-ladder.sh 5 out gnu++14    all flags, C++14
set -e
level=$1
out=$2
std=${3:-gnu++11}
[ -n "$level" ] && [ -n "$out" ] || { echo "usage: $0 LEVEL(0-5) OUTDIR [gnu++98|gnu++11|gnu++14]"; exit 1; }
here=$(cd "$(dirname "$0")" && pwd)
mkdir -p "$out"
out=$(cd "$out" && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT

rsync -a --exclude .git --exclude BUILD "$here"/ "$work"/
git -C "$here" show b8fe2a2:Makefile > "$work/Makefile"

rungs=("-fno-strict-aliasing" "-mno-unaligned-access" "-fwrapv" "-ftrivial-auto-var-init=zero" "-fno-aggressive-loop-optimizations")
extra=""
for ((i=0; i<level; i++)); do extra="$extra '${rungs[$i]}'"; done

cd "$work"
# the fork Makefile has -std=gnu++98 on the CPP line and -std=gnu++11 in CXX_FLAGS (last one wins),
# so always set both
sed -i '' -E -e "s/'-std=gnu\+\+[0-9]+'/'-std=$std'/g" -e "s/CXX_FLAGS \+= -std=gnu\+\+[0-9]+/CXX_FLAGS += -std=$std/" Makefile
if [ -n "$extra" ]; then
    sed -i '' -e "s|'-fno-delete-null-pointer-checks'|'-fno-delete-null-pointer-checks'$extra|g" Makefile
fi

make -j8 > build.log 2>&1 || { grep -E "error" build.log | head -10; exit 1; }
tag="L${level}_${std//+/p}"
cp BUILD/NPR_14.bin "$out/NPR_$tag.bin"
cp BUILD/NPR_14.elf "$out/NPR_$tag.elf"
multi=$(arm-none-eabi-objdump -d BUILD/NPR_14.elf | grep -c -E "\s(ldrd|strd|ldm|stm|ldmia|stmia|vldr|vstr)\b" || true)
printf '%-12s %7s bytes  sha %s  %s  multiword %s\n' "$tag" "$(stat -f %z BUILD/NPR_14.bin)" \
    "$(shasum -a 256 BUILD/NPR_14.bin | cut -c1-12)" \
    "$(arm-none-eabi-size BUILD/NPR_14.elf | tail -1 | awk '{print "text " $1}')" "$multi"
