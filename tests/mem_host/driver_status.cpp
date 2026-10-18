// Case 7: HMI_print_status with every connection state, every rejection reason and several client IDs,
// for client and master. The real function runs with the real 120 byte HMI_out_str. ASan reports a read
// behind the text tables or a write behind HMI_out_str.
#include "mbed.h"
#include "status_stub.h"

char HMI_out_str[120];
static int longest;
void HMI_printf_detail(void) { int n = strlen(HMI_out_str); if (n > longest) longest = n; }
int slow_counter;
bool is_TDMA_master;
int G_uplink_bandwidth_temp, G_downlink_bandwidth_temp, G_temperature_SI4463;
long int TDMA_table_TA[radio_addr_table_size];
unsigned char my_radio_client_ID, CONF_radio_state_ON_OFF, G_connect_rejection_reason, CONF_master_FDD;
unsigned int my_client_radio_connexion_state, RX_top_FDD_up_counter, RSSI_total_stat, RSSI_stat_pkt_nb;
unsigned int G_rx_fec_ok, G_rx_fec_fixed, G_rx_fec_lost, G_rx_seg_gap;
int RX_Eth_IPv4_counter, TX_radio_IPv4_counter, RX_radio_IPv4_counter;
unsigned short int G_downlink_RSSI, G_downlink_BER;
unsigned short int G_radio_addr_table_RSSI[radio_addr_table_size], G_radio_addr_table_BER[radio_addr_table_size];
#include "status_part.inc"

int main() {
	unsigned char ids[] = {0, 6, 7, 126, 127, 128, 254, 255};
	unsigned int states[] = {0, 1, 2, 3, 4, 5, 6, 9, 255, 0xFFFFFFFF};
	for (int master = 0; master < 2; master++)
	for (unsigned int s = 0; s < sizeof(states)/sizeof(states[0]); s++)
	for (int reason = 0; reason < 256; reason++)
	for (unsigned int k = 0; k < sizeof(ids); k++)
	for (int on = 0; on < 2; on++) {
		is_TDMA_master = master;
		my_client_radio_connexion_state = states[s];
		G_connect_rejection_reason = reason;
		my_radio_client_ID = ids[k];
		CONF_radio_state_ON_OFF = on;
		slow_counter = on;
		RSSI_stat_pkt_nb = k & 1;
		HMI_print_status();
	}
	printf("longest line %d of 119\n", longest);
	return 0;
}
