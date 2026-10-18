// Case 4: a new client asks the master for a connection. The real signaling_connect_req_process
// builds the 61 byte ACK in raw_answer. ASan reports the write behind the array when it is too short.
#include "mbed.h"
#include "global_variables.h"
#include "Eth_IPv4.h"

#define radio_addr_table_size 7
Timer GLOBAL_timer;
#include "ip_conv.inc"
unsigned long int CONF_radio_IP_start = 0xC0A80A03, CONF_radio_IP_size = 7;
char CONF_radio_addr_table_status[radio_addr_table_size];
unsigned long int CONF_radio_addr_table_IP_begin[radio_addr_table_size], CONF_radio_addr_table_IP_size[radio_addr_table_size];
char CONF_radio_addr_table_callsign[radio_addr_table_size][16];
unsigned int CONF_radio_addr_table_date[radio_addr_table_size];
char CONF_radio_my_callsign[16] = "xxDL6ABE-70";
LAN_conf_T LAN_conf_applied;
static int added_size;
static unsigned char last_entry[100];
void TDMA_init_TA(unsigned char, int) {}
void signaling_TX_add_entry(unsigned char* raw_data, int size) { added_size = size; memcpy(last_entry, raw_data, size); }
void signaling_frame_push(void) {}
#include "signaling_part.inc"

int main() {
	unsigned char call[16] = "DL6ABE-71";
	LAN_conf_applied.LAN_modem_IP = 0xC0A80A02;
	LAN_conf_applied.LAN_subnet_mask = 0xFFFFFFF0;
	signaling_connect_req_process(call, 1, 0, 0);
	printf("entry of %d bytes, type 0x%02X, size field %d, client ID %d\n", added_size, last_entry[0], last_entry[1], last_entry[2]);
	return (added_size == last_entry[1] + 2) ? 0 : 1;
}
