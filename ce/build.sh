#!/bin/bash
# Builds the NPR-70 firmware against Mbed OS CE (tag mbed-os-7.1.0 in $NPR_CE_WORK/mbed-os-7.1.0, outside the repository).
# Output: $NPR_CE_WORK/build-npr70/npr70_ce.bin (+ .elf, .map). No flashing.
#
# The version is YYMMDD plus the build hour letter, a plus sign and the short commit (NPR FW 261018K+1487529).
# "-dirty" is added when tracked files of npr70/ or npr70-ce/ differ from the commit. NPR_BUILD_STAMP=<YYMMDDX> sets the
# first part by hand (a release build uses the stamp of the release), NPR_GIT_ID=<text> sets the second part.
#
# usage: NPR_CE_WORK=<work dir> [NPR_BUILD_STAMP=...] [NPR_GIT_ID=...] [NPR_CONSOLE_BAUD=115200] [NPR_HAL_TICK_WRAP=ON|OFF] [NPR_SCANF_FLOAT=ON|OFF] [NPR_DIAG=ON|OFF] [BUILD_DIR=...] ./build.sh
[ -n "$NPR_CE_WORK" ] || { echo "set NPR_CE_WORK to the work directory with mbed-os-7.1.0" >&2; exit 1; }
here=$(cd "$(dirname "$0")" && pwd)
build=${BUILD_DIR:-$NPR_CE_WORK/build-npr70}
stamp=${NPR_BUILD_STAMP:-$(date +%y%m%d)$(date +%H | awk '{print substr("ABCDEFGHIJKLMNOPQRSTUVWX", $1 + 1, 1)}')}
if [ -z "${NPR_GIT_ID:-}" ]; then
    NPR_GIT_ID=$(git -C "$here" rev-parse --short=7 HEAD 2>/dev/null) || NPR_GIT_ID=nogit
    # the firmware sources are in ../npr70/source (project repo) or ../source (GitHub repo)
    if [ -n "$(git -C "$here" status --porcelain --untracked-files=no -- "$here" "$here/../npr70" "$here/../source" 2>/dev/null)" ]; then NPR_GIT_ID="$NPR_GIT_ID-dirty"; fi
fi
id="$stamp+$NPR_GIT_ID"
cmake -S "$here" -B "$build" -GNinja -DCMAKE_BUILD_TYPE=Release -DMBED_TARGET=NPR_L432KC -DUPLOAD_METHOD=NONE \
    -DNPR_FW_BUILD_ID="$id" -DNPR_HAL_TICK_WRAP="${NPR_HAL_TICK_WRAP:-ON}" -DNPR_SCANF_FLOAT="${NPR_SCANF_FLOAT:-ON}" -DNPR_DIAG="${NPR_DIAG:-ON}" -DNPR_CONSOLE_BAUD="${NPR_CONSOLE_BAUD:-115200}" -DNPR_UART_IRQ_PRIORITY="${NPR_UART_IRQ_PRIORITY:-OFF}" -DNPR_DBG_NO_ECHO="${NPR_DBG_NO_ECHO:-OFF}" \
    2>&1 | grep -E "^[0-9]\. (IRAM|IROM)|Configuring (done|incomplete)|Error|error" 
ninja -C "$build" 2>&1 | tee "$build/build.log" | grep -vE "^/opt/homebrew|^\[" | grep -E "warning:|error|RAM Bank|ROM Bank|Total (Static|Flash)|undefined|built:" | sed -E "s#$here/\.\./(npr70/)?source/##" | cut -c1-200
echo "version id: $id"
ls -l "$build/npr70_ce.bin" 2>/dev/null | awk '{print $5, $9}'
