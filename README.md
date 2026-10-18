# NPR-70 modem firmware (fork DL6ABE)

Firmware for the NPR-70 packet radio modem (New Packet Radio, 70 cm) designed by F4HDK: STM32L432KC (Nucleo), W5500 Ethernet, Si4463 radio.

- Original project: https://hackaday.io/project/164092-npr-new-packet-radio
- Base of this source: the 2020_06_29 release by F4HDK, through the fork `elafargue/npr70` (Ed Lafargue), which added SNMP support, a refactor of the settings and reduced memory usage.
- This fork: Mbed OS 5.15.9 (the last 5.x release), SNMP client table, console commands `debug`, `who`, `version`, `uptime`, `clear stats`, fixes for memory errors found with host tests (ASan), and a version string `YYMMDD` plus a letter for the build hour (A = 00 to X = 23).

## Build

GCC ARM (`arm-none-eabi-gcc`) and `make`:

    make -j8

Result: `BUILD/NPR_14.bin` and `BUILD/NPR_14.elf`. `mbed-os/` is Mbed OS 5.15.9, cut down to the STM32L4 targets, and is not to be changed. The firmware needs three adjustments to it, all in this project:

- `main.cpp` selects the ALT0 pins for `spi_2` (since Mbed OS 5.8.6, PB_5/PB_4/PB_3 map to SPI1, the peripheral of the radio).
- `source/hal_tick_wrap.cpp` replaces `HAL_GetTick` (`-Wl,--wrap,HAL_GetTick`). Without it, the SPI timeouts break from Mbed OS 5.9.1 on.
- The Makefile uses `-std=gnu++14` and `-Os`.

Check the pin map of `PeripheralPins.c` before updating Mbed OS.

## Flash

Use OpenOCD. `st-flash` cannot run its flash loader after a firmware built with Mbed OS 5.11 or later. Flash the first build of a new Mbed OS version without antenna and watch the TX LED.

## Tests

Host tests (no hardware), each with `run.sh`:

- `tests/snmp_host/` SNMP agent (needs net-snmp tools)
- `tests/dhcp_host/` and `tests/hmi_host/` DHCP server and `set` command, against the frozen original code
- `tests/mem_host/` memory errors (ASan/UBSan)

## SNMP

SNMP v1, community `public`, read only. `NPR-70.mib` describes the vendor OIDs (`1.3.6.1.4.1.54539.100`). `snmpwalk` without an OID only walks `mib-2`, use `1.3.6.1.4.1.54539`.

## Status

The build `261017I` (Mbed OS 5.15.9, tag `release-261017I-mbed-5.15.9`) is the stable build of this fork and the rollback point. Use it at your own risk. The firmware transmits on amateur radio frequencies and needs a valid licence.

## License

GNU General Public License v3 (`gpl-3.0.txt`). Mbed OS is Apache-2.0 and keeps its own license files in `mbed-os/`.

Credit: F4HDK (original design and firmware), Ed Lafargue (SNMP, settings, memory), DL6ABE (this fork).
