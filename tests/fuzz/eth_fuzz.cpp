// libFuzzer target for the IP forwarding code: IPv4_to_radio (frame from the Ethernet) and IPv4_from_radio (packet
// from the radio) of Eth_IPv4.cpp, cut out by tools/fuzz.sh like in mem_host.
// Byte 0: bit 0 = 1 from the radio, bit 1 = master, bit 2 = connected, bit 3 = default route active, bits 4 to 5 = number
//   of clients in the table. The rest is the Ethernet frame. It lives in a heap block of exactly its size, so ASan
//   reports every read or write behind the frame. IPv4_to_radio gets 34 to 1598 bytes, as Eth_RX_dequeue guarantees
//   (IPv4 with a length field and 36 bytes at least); IPv4_from_radio up to 1600 bytes, a full row of the radio buffer.
#include "mbed.h"
#include "W5500.h"

#define radio_addr_table_size 7
struct LAN_conf_T {
    unsigned long int LAN_modem_IP, DHCP_range_start, DHCP_range_size, LAN_subnet_mask, LAN_def_route;
    unsigned char LAN_def_route_activ, LAN_DNS_activ;
    unsigned long int LAN_DNS_value;
    unsigned char DHCP_server_active;
};
Timer GLOBAL_timer;
LAN_conf_T LAN_conf_applied;
bool is_TDMA_master;
unsigned char CONF_master_FDD, CONF_modem_MAC[6] = {1, 2, 3, 4, 5, 6}, my_radio_client_ID = 1;
unsigned long int CONF_radio_IP_start = 0xC0A80A03, CONF_radio_IP_size = 7;
unsigned long int CONF_radio_addr_table_IP_begin[radio_addr_table_size], CONF_radio_addr_table_IP_size[radio_addr_table_size];
unsigned int my_client_radio_connexion_state = 2;
int RX_Eth_IPv4_counter;
unsigned int G_eth_tx_ok, G_eth_tx_wait, G_radio_unrouted;
W5500_chip* W5500_p1;

static volatile unsigned char sink;
static void read_all(const unsigned char* d, int n) { for (int i = 0; i < n; i++) sink ^= d[i]; }   // ASan checks the length
void ARP_RX_packet_treatment(unsigned char*, int) {}
void FDDdown_RX_pckt_treat(unsigned char*, int) {}
void segment_and_push(unsigned char* data, int size, unsigned char, unsigned char) {
    if (size < 0 || size > 1600) __builtin_trap();                  // segment_and_push reads size bytes
    read_all(data, size);
}
static int mac_calls;
int lookfor_MAC_from_IP(unsigned char* frame, unsigned long int) { frame[0] = 0; return ++mac_calls & 1; }   // alternate: found, not found
void W5500_write_TX_buffer(W5500_chip*, uint8_t, unsigned char* data, int size, int) { read_all(data, size); }
int W5500_read_received_size_stub;
void IPv4_to_radio(unsigned char*, int);
void IPv4_from_radio(unsigned char*, int);
void flush_temp_Eth_buffer(unsigned long int);
unsigned char lookfor_client_ID_from_IP(unsigned long int);

#include "eth_part.inc"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size < 1) return 0;
    bool from_radio = data[0] & 1;
    is_TDMA_master = (data[0] >> 1) & 1;
    my_client_radio_connexion_state = (data[0] & 4) ? 2 : 1;
    LAN_conf_applied.LAN_def_route_activ = (data[0] >> 3) & 1;
    int known = (data[0] >> 4) & 3;
    for (int i = 0; i < radio_addr_table_size; i++) {
        CONF_radio_addr_table_IP_begin[i] = i < known ? 0xC0A80A03 + i : 0;
        CONF_radio_addr_table_IP_size[i] = i < known ? 1 : 0;
    }
    LAN_conf_applied.LAN_modem_IP = 0xC0A80A02;
    LAN_conf_applied.LAN_subnet_mask = 0xFFFFFFF0;
    LAN_conf_applied.LAN_def_route = 0xC0A80A01;
    LAN_conf_applied.DHCP_range_start = 0xC0A80A03;
    LAN_conf_applied.DHCP_range_size = 1;
    size_t n = size - 1;
    if (n > 1600) n = 1600;
    if (!from_radio && (n < 34 || n > 1598)) return 0;
    unsigned char *frame = (unsigned char *)malloc(n ? n : 1);
    memcpy(frame, data + 1, n);
    if (from_radio) IPv4_from_radio(frame, (int)n);
    else IPv4_to_radio(frame, (int)n);
    free(frame);
    return 0;
}
