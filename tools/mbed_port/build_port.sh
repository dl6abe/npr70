#!/bin/bash
# Build the firmware against another mbed-os version, outside the project tree.
#
# usage: ./build_port.sh TAG [WORK] [OPT]
#   TAG   mbed-os version, e.g. 5.8.6 (export_mbed.sh must have run for it)
#   WORK  working directory of export_mbed.sh, default ~/.cache/npr70-mbed
#   OPT   optimisation flag replacing -O0 in the Makefile, e.g. -Os (default: keep -O0)
#   env EXTRA_CFLAGS  extra C compiler flags (see below)
#   env HAL_TICK      1 or 2: wrap HAL_GetTick with source/hal_tick_wrap.cpp
#   env BUILD_NAME    name of the build directory (default TAG), to build variants of one tag side by side
# result: WORK/build-TAG/BUILD/NPR_14.bin  (not flashed, not copied to firmware-own-builds)
set -e
tag=${1:?usage: build_port.sh TAG [WORK] [OPT]}
work=${2:-$HOME/.cache/npr70-mbed}
opt=${3:-}
here=$(cd "$(dirname "$0")" && pwd)
npr70=$(cd "$here/../.." && pwd)
dir="$work/build-${BUILD_NAME:-$tag}"

rm -rf "$dir"
mkdir -p "$dir"
rsync -a --exclude BUILD --exclude mbed-os --exclude tests --exclude tools "$npr70/" "$dir/"
ln -s "$work/mbed-os-$tag" "$dir/mbed-os"
python3 "$here/port_makefile.py" "$work/export-$tag" "$dir/port" "$npr70"
cp "$dir/port/Makefile" "$dir/Makefile"
cp "$dir/port/mbed_config.h" "$dir/mbed_config.h"
[ -n "$opt" ] && sed -i.bak "s/-O0/$opt/g" "$dir/Makefile" && rm -f "$dir/Makefile.bak"
# Which peripheral do PB_5/PB_4/PB_3 select in this mbed-os tree? Up to 5.8.5 SPI3 (plain names), from 5.8.6
# on SPI1 (the radio is on SPI1 too), then the _ALT0 names select SPI3. main.cpp reads NPR_SPI2_ALT0.
pins="$dir/mbed-os/targets/TARGET_STM/TARGET_STM32L4/TARGET_STM32L432xC/TARGET_NUCLEO_L432KC/PeripheralPins.c"
if grep -Eq '\{PB_5, *SPI_1' "$pins" 2>/dev/null; then alt0=1; else alt0=0; fi
printf 'CXX_FLAGS += -DNPR_SPI2_ALT0=%s\n' "$alt0" >> "$dir/Makefile"
echo "NPR_SPI2_ALT0=$alt0"
# mbed_rtx_conf.h sets OS_DYNAMIC_MEM_SIZE to 0. In the 5.9 development trees RTX_Config.c does not see that
# file with our include order and falls back to 4096, a static array os_mem in RAM (5.9.0: 4 KB, no boot).
printf 'C_FLAGS += -DOS_DYNAMIC_MEM_SIZE=0\nCXX_FLAGS += -DOS_DYNAMIC_MEM_SIZE=0\n' >> "$dir/Makefile"
# mbed-os 5.12 and later: the SPI state moved from SPI::_spi to SPI::_peripheral->spi (source/SPI_F4HDK.cpp)
if grep -q 'spi_t _spi;' "$dir/mbed-os/drivers/SPI.h" 2>/dev/null; then spi_p=0; else spi_p=1; fi
printf 'CXX_FLAGS += -DNPR_SPI_PERIPHERAL=%s\n' "$spi_p" >> "$dir/Makefile"
echo "NPR_SPI_PERIPHERAL=$spi_p"
# mbed-os 5.13 and later are built with -std=gnu++14 (tools/profiles/release.json); the project says gnu++11
if grep -q 'gnu++14' "$dir/mbed-os/tools/profiles/release.json" 2>/dev/null; then
    printf 'CXX_FLAGS += -std=gnu++14\n' >> "$dir/Makefile"
    echo "C++14"
fi
# mbed-os 5.14 and later: hal/source/mbed_gpio.c lists the Arduino pins D0 to D15, NUCLEO_L432KC (Nano layout)
# has D0 to D13 only. The two missing names become NC (a table entry for no pin).
names="$dir/mbed-os/targets/TARGET_STM/TARGET_STM32L4/TARGET_STM32L432xC/TARGET_NUCLEO_L432KC/PinNames.h"
if grep -q '{D14, DEFAULT_GPIO' "$dir/mbed-os/hal/mbed_gpio.c" 2>/dev/null && ! grep -Eq '\bD14\b' "$names"; then
    printf 'C_FLAGS += -DD14=NC -DD15=NC\n' >> "$dir/Makefile"
    echo "D14 and D15 set to NC"
fi
# HAL_TICK=1 or 2 replaces HAL_GetTick (source/hal_tick_wrap.cpp, needed from mbed-os 5.9.1 on)
if [ -n "$HAL_TICK" ]; then
    printf 'CXX_FLAGS += -DNPR_HAL_TICK=%s\nLD_FLAGS += -Wl,--wrap,HAL_GetTick\n' "$HAL_TICK" >> "$dir/Makefile"
    echo "HAL_TICK=$HAL_TICK (HAL_GetTick wrapped)"
fi
# mbed-os 5.9.0 calls memcpy without <string.h> (fixed in later tags). GCC 14 and later make that an
# error. EXTRA_CFLAGS, e.g. EXTRA_CFLAGS=-Wno-error=implicit-function-declaration, avoids editing mbed-os.
[ -n "$EXTRA_CFLAGS" ] && printf 'C_FLAGS += %s\n' "$EXTRA_CFLAGS" >> "$dir/Makefile"

cd "$dir"
make -j8 2>&1 | grep -E ' error |\*\*\*|undefined|multiple definition|bin file' || true
arm-none-eabi-size BUILD/NPR_14.elf | tail -1
bytes=$(wc -c < BUILD/NPR_14.bin | tr -d " ")
echo "bin $bytes bytes, free before the config at 0x0803D000 (249856): $((249856 - bytes))"
shasum -a 256 BUILD/NPR_14.bin
