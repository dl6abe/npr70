// libFuzzer target for the signaling frames from the radio: signaling_frame_exploitation with WHOIS, connect request,
// ACK, NACK and disconnect (signaling.cpp, cut out by tools/fuzz.sh like in mem_host).
// Byte 0: bit 0 = master, bits 1 to 3 = number of clients already in the table (master), bit 4 = state of the client
//   (connected), bit 5 = default route active. Byte 1: TA value. Then records of [length byte][frame] (the frame as it
//   comes out of the FEC: byte 0 address, byte 1 protocol, then the fields from byte 2). All records go to one modem.
#include "mbed.h"
#include "global_variables.h"

#define radio_addr_table_size 7
Timer GLOBAL_timer;
#include "ip_conv.inc"
unsigned long int CONF_radio_IP_start = 0xC0A80A03, CONF_radio_IP_size = 7;
char CONF_radio_addr_table_status[radio_addr_table_size];
unsigned long int CONF_radio_addr_table_IP_begin[radio_addr_table_size], CONF_radio_addr_table_IP_size[radio_addr_table_size];
char CONF_radio_addr_table_callsign[radio_addr_table_size][16];
unsigned int CONF_radio_addr_table_date[radio_addr_table_size];
unsigned short int G_radio_addr_table_RSSI[radio_addr_table_size], G_radio_addr_table_BER[radio_addr_table_size];
long int TDMA_table_TA[radio_addr_table_size];
char CONF_radio_my_callsign[16] = "DL6ABE-72";
char CONF_radio_master_callsign[16];
LAN_conf_T LAN_conf_applied;
bool is_TDMA_master;
unsigned char my_radio_client_ID, G_connect_rejection_reason;
int my_client_radio_connexion_state;
static unsigned char last_entry[100];
void TDMA_init_TA(unsigned char, int) {}
void TDMA_NULL_frame_init(int) {}
void HMI_force_exit(void) {}
void reset_DHCP_table(LAN_conf_T*) {}
void W5500_re_configure(unsigned char) {}
void signaling_frame_push(void) {}
void signaling_disconnect_ACK_TX(unsigned, unsigned char*) {}
void signaling_TX_add_entry(unsigned char* raw_data, int size) {
    if (size > 0 && size <= (int)sizeof(last_entry)) memcpy(last_entry, raw_data, size);
    if (size != raw_data[1] + 2) __builtin_trap();                  // the size field must match the entry
}
int connect_state_machine_counter, time_counter_last_ack;
unsigned long int signaling_lookfor_IP_range(unsigned long int req_size);
void signaling_connect_req_process(unsigned char*, unsigned long int, unsigned char, int);
void signaling_connect_ACK_process(unsigned char*);
void signaling_connect_NACK_process(unsigned char);
void signaling_disconnect_req_process(unsigned, unsigned char*);
void signaling_disconnect_ACK_process(unsigned, unsigned char*);
void signaling_whois_interpret(unsigned char, unsigned char*, unsigned long int, unsigned long int,
                               unsigned char, unsigned short int, short int);
#include "signaling_part.inc"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size < 2) return 0;
    is_TDMA_master = data[0] & 1;
    int known = (data[0] >> 1) & 7;
    for (int i = 0; i < radio_addr_table_size; i++) {
        CONF_radio_addr_table_status[i] = i < known;
        CONF_radio_addr_table_IP_begin[i] = 0xC0A80A03 + i;
        CONF_radio_addr_table_IP_size[i] = 1;
        snprintf(CONF_radio_addr_table_callsign[i], 16, "DL6ABE-%d", 70 + i);
    }
    my_client_radio_connexion_state = (data[0] & 0x10) ? 2 : 1;
    my_radio_client_ID = 1;
    LAN_conf_applied.LAN_modem_IP = 0xC0A80A02;
    LAN_conf_applied.LAN_subnet_mask = 0xFFFFFFF0;
    LAN_conf_applied.LAN_def_route_activ = (data[0] >> 5) & 1;
    int ta = (signed char)data[1];
    size_t i = 2;
    while (i < size) {
        size_t n = data[i++];
        if (n > size - i) n = size - i;
        unsigned char frame[360];                                     // like data_RX of L1L2_radio.cpp
        memset(frame, 0, sizeof(frame));
        memcpy(frame, data + i, n);
        i += n;
        signaling_frame_exploitation(frame, (int)n, ta);
    }
    return 0;
}
