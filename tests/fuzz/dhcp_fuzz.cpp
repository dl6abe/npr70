// libFuzzer target for the DHCP server (DHCP part of DHCP_ARP.cpp, cut out by tools/fuzz.sh like in dhcp_host).
// Input: byte 0 selects the configuration (range size, default route, DNS), then records of
// [length in units of 4 bytes][UDP payload]. All records go to one server, so the lease table carries over.
#include "mbed.h"
#include "global_variables.h"
#include "W5500.h"
#include "Eth_IPv4.h"
#include "DHCP_new.cpp"

Timer GLOBAL_timer;
#include "ip_conv.inc"

static unsigned char pending[600];
static int pending_size = 0;

uint16_t W5500_read_received_size(W5500_chip*, uint8_t) { return pending_size; }
uint32_t W5500_read_UDP_pckt(W5500_chip*, uint8_t, unsigned char* data, uint32_t len) {
    int n = pending_size < (int)len ? pending_size : (int)len;
    memcpy(data, pending, n);
    pending_size = 0;
    return n;
}
void W5500_write_TX_buffer(W5500_chip*, uint8_t, unsigned char* data, int size, int) {
    volatile unsigned char sink = 0;
    for (int i = 0; i < size; i++) sink ^= data[i];       // read the whole answer: ASan checks its length
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size < 1) return 0;
    W5500_chip chip;
    LAN_conf_T lan;
    memset(&lan, 0, sizeof(lan));
    lan.LAN_modem_IP = 0xC0A80A02;
    lan.DHCP_range_start = 0xC0A80A03;
    lan.DHCP_range_size = (data[0] & 3) == 0 ? 7 : ((data[0] & 3) == 1 ? 3 : ((data[0] & 3) == 2 ? 40 : 1));
    lan.LAN_subnet_mask = 0xFFFFFFF0;
    lan.LAN_def_route = 0xC0A80A01;
    lan.LAN_def_route_activ = (data[0] >> 2) & 1;
    lan.LAN_DNS_activ = (data[0] >> 3) & 1;
    lan.LAN_DNS_value = 0x08080808;
    reset_DHCP_table(&lan);
    size_t i = 1;
    while (i < size) {
        size_t n = (size_t)data[i++] * 4;       // length in units of 4 bytes: a DHCP message is longer than 255 bytes
        if (n > size - i) n = size - i;
        if (n > sizeof(pending)) n = sizeof(pending);
        memset(pending, 0, sizeof(pending));
        memcpy(pending, data + i, n);
        i += n;
        pending_size = (int)n;
        DHCP_server(&lan, &chip);
    }
    return 0;
}
