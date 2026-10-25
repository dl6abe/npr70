# NPR-70 modem firmware (fork DL6ABE)

Firmware for the NPR-70 packet radio modem (New Packet Radio, 70 cm) designed by F4HDK: STM32L432KC (Nucleo), W5500 Ethernet, Si4463 radio.

- Original project: https://hackaday.io/project/164092-npr-new-packet-radio
- Base of this source: the 2020_06_29 release by F4HDK, through the fork `elafargue/npr70` (Ed Lafargue), which added SNMP support, a refactor of the settings and reduced memory usage.
- This fork: Mbed OS CE 7.1.0 (the last build on Mbed OS 5.15.9 is the rollback), SNMP client table, console commands `debug`, `who`, `version`, `uptime`, `clear stats`, `stack`, `latency`, `buffers`, `txlat`, `ta`, fixes for memory errors found with host tests (ASan), and a version string `YYMMDD` plus a letter for the build hour (A = 00 to X = 23).

## Build

The firmware builds against Mbed OS CE 7.1.0 (https://github.com/mbed-ce/mbed-os) with CMake. Everything specific to Mbed OS CE is in `ce/`: `CMakeLists.txt`, the target `NPR_L432KC` (`custom_targets.json5`), the replacement for `Serial` (`compat/npr_ce_compat.h`) and `build.sh`. The sources in `source/` are shared.

Requirements (tested with these versions): CMake 4.4, Ninja 1.13, `arm-none-eabi-gcc` 15.3, Python 3.15, Git. The sources are C++17 (`-std=gnu++17`, set in `ce/CMakeLists.txt`; the host tests use the same standard). The checkout of Mbed OS CE needs about 1 GB with its submodules and is not part of this repository.

    git clone --depth 1 --branch mbed-os-7.1.0 --recurse-submodules --shallow-submodules \
        https://github.com/mbed-ce/mbed-os $NPR_CE_WORK/mbed-os-7.1.0
    NPR_CE_WORK=<work dir> ce/build.sh

Result: `$NPR_CE_WORK/build-npr70/npr70_ce.bin` (about 133 kB, 52 % of the flash, 94 % of the first RAM bank). Options of `build.sh` (environment variables): `NPR_CONSOLE_BAUD` (115200), `NPR_HAL_TICK_WRAP` (ON), `NPR_SCANF_FLOAT` (ON), `BUILD_DIR`.

- The console UART runs at 115200 baud, fixed at build time.
- `source/hal_tick_wrap.cpp` replaces `HAL_GetTick` (`-Wl,--wrap,HAL_GetTick`). Without it, the SPI timeouts break.
- The settings in flash keep their layout. A flash with the CE build keeps them.
- The binary does not depend on the build directory: source paths are mapped to fixed names (`-ffile-prefix-map`), so two builds of the same sources are equal, apart from the version string.

The Mbed OS 5.15.9 line is frozen. This source does not build with it any more. The last build on 5.15.9 is `261017I`, the tag `release-261017I-mbed-5.15.9` holds its sources with the Makefile and `mbed-os/`.

Known limit: The ST-Link virtual COM port stops forwarding after a host write of 16 bytes or more. Replug the USB cable. Use telnet for input.

## Flash

Use OpenOCD. `st-flash` cannot run its flash loader after a firmware built with Mbed OS 5.11 or later. Flash the first build of a new Mbed OS version without antenna and watch the TX LED.

## Tests

Host tests (no hardware), each with `run.sh`:

- `tests/snmp_host/` SNMP agent (needs net-snmp tools)
- `tests/dhcp_host/` and `tests/hmi_host/` DHCP server and `set` command, against the frozen original code
- `tests/ip_host/` checks of IP addresses and the netmask in `set` and `save`
- `tests/mem_host/` memory errors (ASan/UBSan)
- `tests/diag_host/` latency statistics and stack evaluation of the `stack` and `latency` commands
- `tests/chrono_host/` conversion of `Timer::read_us()` to `elapsed_time().count()`
- `tests/fuzz/` libFuzzer targets for the parsers (SNMP, DHCP, radio, signaling, Ethernet, W5500, ARP)

## SNMP

SNMP v1, community `public`, read only. `NPR-70.mib` describes the vendor OIDs (`1.3.6.1.4.1.54539.100`). `snmpwalk` without an OID only walks `mib-2`, use `1.3.6.1.4.1.54539`.

## Status

The build `261017I` (Mbed OS 5.15.9, tag `release-261017I-mbed-5.15.9`) is the stable build of this fork and the rollback point. The Mbed OS CE 7.1.0 line is the standard now: the release `release-261025H-mbed-ce-7.1.0` (build `261025H`) replaces the pre-release `release-261018L-mbed-ce-7.1.0`. Use both at your own risk. The firmware transmits on amateur radio frequencies and needs a valid licence.

## License

GNU General Public License v3 (`gpl-3.0.txt`). Mbed OS is Apache-2.0 and keeps its own license files in its checkout.

Credit: F4HDK (original design and firmware), Ed Lafargue (SNMP, settings, memory), DL6ABE (this fork).
