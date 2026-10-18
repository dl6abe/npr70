#!/bin/bash
# Host tests for memory errors found in NETWORK_DEBUG_LOG.md 2.36. Each case builds the real firmware
# functions (cut out of the sources) with a fake chip or fake callbacks and runs them under ASan/UBSan.
#   1  oversized UDP packet on the DHCP socket   (DHCP_ARP.cpp, W5500.cpp)
#   2  more than 6 radio segments of one packet  (L1L2_radio.cpp)
#   3  master callsign in the ACK without end    (signaling.cpp)
#   8  Ethernet frame of any size in the 1600 byte buffer (Eth_IPv4.cpp, W5500.cpp)
#  10a packet shorter than 63 bytes: padding read from behind the source buffer (L1L2_radio.cpp)
#  10b IP length from the radio larger than the received bytes (Eth_IPv4.cpp)
#   6  client ID outside 0..6 in the ACK, and as table index in the status display
#   7  status display with unknown state or rejection reason
#   4  connect ACK is 61 bytes, buffer was 60    (signaling.cpp)
#  11  PHY self-heal and re-configure watchdog   (W5500.cpp, fake PHY register and clock)
#  12  NACK to a connected client: ID back to 0x7E (signaling.cpp, NETWORK_DEBUG_LOG.md 2.77)
# All cases must pass. A bug that is open again can be marked as expected failure (XFAIL) with
# EXPECT<n>=fail, for example EXPECT2=fail. A different result than expected makes the script exit with 1.
set -u
here=$(cd "$(dirname "$0")" && pwd)
src="$here/../../source"
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
cp "$here"/stubs/*.h "$here"/*.cpp "$here"/*.h "$work"/ 2>/dev/null
cp "$src/W5500.h" "$work"/
# IP_char2int / IP_int2char from Eth_IPv4.cpp
python3 - "$src" "$work" <<'PY'
import sys, re
src, work = sys.argv[1:3]
def lines(f): return open(src + "/" + f, newline='').read().replace('\r\n', '\n').split('\n')
def cut(f, start, end):
    L = lines(f)
    a = next(i for i, l in enumerate(L) if l.lstrip().startswith(start))
    b = next(i for i in range(a + 1, len(L)) if L[i].lstrip().startswith(end))
    return '\n'.join(L[a:b]) + '\n'
open(work + "/ip_conv.inc", "w").write(cut("Eth_IPv4.cpp", "unsigned long int IP_char2int", "//static unsigned char match_RTP_value"))
# DHCP part and the UDP read functions
w = cut("W5500.cpp", "void W5500_read_long", "int W5500_read_MAC_pckt")
w = w[:w.rindex("/**")]          # drop the comment of the next function
open(work + "/W5500_part.inc", "w").write(w)
open(work + "/DHCP_part.inc", "w").write('#include "W5500_part.inc"\n#include "DHCP_decl.h"\n' + cut("DHCP_ARP.cpp", "#define DHCP_ARP_tab_size", "void DHCP_ARP_print_entries"))
open(work + "/L1L2_part.inc", "w").write(cut("L1L2_radio.cpp", "void radio_RX_FIFO_dequeue", "void radio_signalisation_frame_building")
    + cut("L1L2_radio.cpp", "int FEC_encode2", "int size_w_FEC_compute"))
def tail(f, start):
    L = lines(f)
    a = next(i for i, l in enumerate(L) if l.lstrip().startswith(start))
    return '\n'.join(L[a:]) + '\n'
open(work + "/eth_part.inc", "w").write(tail("Eth_IPv4.cpp", "static unsigned char temp_Eth_buffer"))
open(work + "/W5500_mac.inc", "w").write(cut("W5500.cpp", "int W5500_read_MAC_pckt", "void W5500_write_TX_buffer"))
open(work + "/pad_part.inc", "w").write(cut("L1L2_radio.cpp", "int size_w_FEC_compute", "//unsigned char TX_buff_intern_FIFOdata[42][384];"))
open(work + "/status_part.inc", "w").write(cut("HMI_telnet.cpp", "void HMI_print_status", "void HMI_periodic_call"))
open(work + "/ack_part.inc", "w").write(cut("signaling.cpp", "void signaling_connect_ACK_process", "void signaling_connect_NACK_process"))
open(work + "/nack_part.inc", "w").write(cut("signaling.cpp", "void signaling_connect_NACK_process", "static unsigned char loc_data"))
open(work + "/signaling_part.inc", "w").write(cut("signaling.cpp", "unsigned long int signaling_lookfor_IP_range", "void signaling_connect_ACK_process"))
open(work + "/phy_part.inc", "w").write(cut("W5500.cpp", "static int W5500_configured", "void W5500_initial_configure"))
PY
cat > "$work/DHCP_decl.h" <<'H'
#include "global_variables.h"
H
cat > "$work/global_variables.h" <<'H'
// Host stub: LAN configuration, the only global the cut-out code reads.
#ifndef STUB_GLOBAL_VARIABLES_H
#define STUB_GLOBAL_VARIABLES_H
#include "mbed.h"
struct LAN_conf_T {
	unsigned long int LAN_modem_IP, DHCP_range_start, DHCP_range_size, LAN_subnet_mask, LAN_def_route;
	unsigned char LAN_def_route_activ, LAN_DNS_activ;
	unsigned long int LAN_DNS_value;
	unsigned char DHCP_server_active;
};
extern LAN_conf_T LAN_conf_applied;
extern Timer GLOBAL_timer;
extern unsigned long int CONF_radio_IP_start, CONF_radio_IP_size;
extern char CONF_radio_my_callsign[16];
#endif
H
printf '#ifndef STUB_ETH_IPV4_H\n#define STUB_ETH_IPV4_H\nunsigned long int IP_char2int(unsigned char*);\nvoid IP_int2char(unsigned long int, unsigned char*);\n#endif\n' > "$work/Eth_IPv4.h"
printf '#ifndef STUB_HMI_H\n#define STUB_HMI_H\n#endif\n' > "$work/HMI_telnet.h"
printf '#ifndef STUB_DHCP_H\n#define STUB_DHCP_H\n#endif\n' > "$work/DHCP_ARP.h"
flags="-std=gnu++11 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -Wall -Wno-unused-function -Wno-unused-variable -I$work"
fail=0
# name, driver, expected (pass|fail), run arguments
run_case() {
	local name=$1 drv=$2 expect=$3; shift 3
	export OUT="$work/$drv.out" ERR="$work/$drv.err"
	if ! c++ $flags -o "$work/$drv" "$work/driver_$drv.cpp" 2>"$work/$drv.build"; then echo "case $name: BUILD FAILED"; head -20 "$work/$drv.build"; fail=1; return; fi
	sh -c '"$0" "$@" >"$OUT" 2>"$ERR"' "$work/$drv" "$@" 2>/dev/null; local rc=$?
	[ -n "${VERBOSE:-}" ] && grep -m6 -E "ERROR|WRITE|READ|#[0-3] " "$work/$drv.err"
	local got=pass; [ $rc -ne 0 ] && got=fail
	local what=$(grep -m1 -oE "ERROR: AddressSanitizer: [a-z-]+|runtime error: [^:]*|MISMATCH" "$work/$drv.err" "$work/$drv.out" | head -1 | sed 's/^[^:]*://')
	if [ "$got" = "$expect" ]; then
		if [ "$expect" = pass ]; then echo "case $name: PASS"; else echo "case $name: XFAIL (open bug: ${what:-exit $rc})"; fi
	else
		echo "case $name: UNEXPECTED $got (expected $expect) ${what}"; cat "$work/$drv.out"; head -12 "$work/$drv.err"; fail=1
	fi
}
run_case "1 DHCP oversized UDP" udp "${EXPECT1:-pass}"
run_case "2 reassembly 8 segments" reasm "${EXPECT2:-pass}" 8
run_case "2b reassembly 6 segments (own sender maximum)" reasm pass 6
run_case "3+6 ACK: callsign without end, client ID outside table" ack "${EXPECT3:-pass}"
run_case "7 status display states and reasons" status "${EXPECT7:-pass}"
run_case "8+10b Ethernet frames, IP length" eth "${EXPECT8:-pass}"
run_case "10a padding of short packets" pad "${EXPECT10:-pass}"
run_case "4 connect ACK size" connect "${EXPECT4:-pass}"
run_case "11 PHY self-heal" phy "${EXPECT11:-pass}"
run_case "12 NACK resets the client ID" nack "${EXPECT12:-pass}"
[ $fail -eq 0 ] && echo "mem host tests as expected" || { echo "mem host tests: UNEXPECTED RESULT"; exit 1; }
