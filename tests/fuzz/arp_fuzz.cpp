// libFuzzer target for the ARP part and the address table of DHCP_ARP.cpp: ARP_RX_packet_treatment (ARP_proxy,
// ARP_client_answer_treatment), lookfor_MAC_from_IP and DHCP_ARP_periodic_free_table (cut out by tools/fuzz.sh).
// Byte 0: bit 0 = master, bit 1 = DHCP server active, bits 2 to 3 = default route / client range variants.
// Then records [type][length][bytes]: type 0 = ARP frame (heap block of exactly that size, Ethernet header and ARP
// packet as Eth_RX_dequeue passes it, at least 14 bytes), type 1 = lookfor_MAC_from_IP(IP in bytes 0 to 3),
// type 2 = DHCP_ARP_periodic_free_table, type 3 = set the clock (4 bytes, microseconds / 4096).
#include "mbed.h"
#include "W5500.h"
#include "global_variables.h"
#include "Eth_IPv4.h"
#include "DHCP_decl.h"

Timer GLOBAL_timer;
LAN_conf_T LAN_conf_applied;
bool is_TDMA_master;
unsigned char CONF_modem_MAC[6] = {1, 2, 3, 4, 5, 6};
unsigned long int CONF_radio_IP_start = 0xC0A80A03, CONF_radio_IP_size = 7;
W5500_chip* W5500_p1;
static volatile unsigned char sink;
void W5500_write_TX_buffer(W5500_chip*, uint8_t, unsigned char* data, int size, int) {
    for (int i = 0; i < size; i++) sink ^= data[i];                 // ASan checks the length of the answer
}
uint16_t W5500_read_received_size(W5500_chip*, uint8_t) { return 0; }
uint32_t W5500_read_UDP_pckt(W5500_chip*, uint8_t, uint8_t*, uint32_t) { return 0; }
void flush_temp_Eth_buffer(unsigned long int) {}
#include "ip_conv.inc"
#include "DHCP_part.inc"
#include "ARP_part.inc"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size < 1) return 0;
    is_TDMA_master = data[0] & 1;
    LAN_conf_applied.DHCP_server_active = (data[0] >> 1) & 1;
    LAN_conf_applied.LAN_modem_IP = 0xC0A80A02;
    LAN_conf_applied.LAN_subnet_mask = 0xFFFFFFF0;
    LAN_conf_applied.DHCP_range_start = 0xC0A80A03;
    LAN_conf_applied.DHCP_range_size = (data[0] & 4) ? 3 : 1;
    stub_timer_fixed = 1;
    stub_timer_us = 0;
    LAN_conf_T lan = LAN_conf_applied;
    reset_DHCP_table(&lan);
    size_t i = 1;
    while (i + 1 < size) {
        int type = data[i] & 3;
        size_t n = data[i + 1];
        i += 2;
        if (n > size - i) n = size - i;
        const uint8_t *rec = data + i;
        i += n;
        if (type == 0) {
            if (n < 14) continue;
            unsigned char *frame = (unsigned char *)malloc(n);
            memcpy(frame, rec, n);
            ARP_RX_packet_treatment(frame, (int)n);
            free(frame);
        } else if (type == 1 && n >= 4) {
            unsigned char mac[6];
            lookfor_MAC_from_IP(mac, IP_char2int((unsigned char*)rec));
        } else if (type == 2) {
            DHCP_ARP_periodic_free_table();
        } else if (type == 3 && n >= 4) {
            stub_timer_us = ((unsigned)rec[0] | ((unsigned)rec[1] << 8) | ((unsigned)rec[2] << 16)) << 8;
        }
    }
    return 0;
}
