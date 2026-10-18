// Cases 3 and 6: connect ACK from the master whose master callsign field has no end marker within 16 bytes.
// The real signaling_connect_ACK_process copies it into CONF_radio_master_callsign[16]. ASan reports
// a write behind the array when the copy is not bounded.
#include "mbed.h"
#include "global_variables.h"
#include "Eth_IPv4.h"

#define radio_addr_table_size 7

Timer GLOBAL_timer;
#include "ip_conv.inc"
char CONF_radio_master_callsign[16];
LAN_conf_T LAN_conf_applied;
unsigned char my_radio_client_ID;
int my_client_radio_connexion_state, connect_state_machine_counter, time_counter_last_ack;
void TDMA_NULL_frame_init(int) {}
void HMI_force_exit(void) {}
void reset_DHCP_table(LAN_conf_T*) {}
void W5500_re_configure(unsigned char) {}
#include "ack_part.inc"

int main() {
	unsigned char raw_data[300];
	memset(raw_data, 'A', sizeof(raw_data));
	raw_data[sizeof(raw_data) - 1] = 0; // the source ends, only the target is too small
	raw_data[0] = 3;
	signaling_connect_ACK_process(raw_data);
	printf("master callsign '%s', client ID %d\n", CONF_radio_master_callsign, my_radio_client_ID);
	int ok = (strlen(CONF_radio_master_callsign) <= 15 && my_radio_client_ID == 3);
	// an ID outside the client table must be ignored: the ID is used as table index
	unsigned char ids[] = {7, 126, 200, 255};
	for (unsigned int k = 0; k < sizeof(ids); k++) {
		raw_data[0] = ids[k];
		my_client_radio_connexion_state = 1;
		signaling_connect_ACK_process(raw_data);
		printf("ACK with client ID %d: own ID %d, state %d\n", ids[k], my_radio_client_ID, my_client_radio_connexion_state);
		if (my_radio_client_ID != 3 || my_client_radio_connexion_state != 1) ok = 0;
	}
	return ok ? 0 : 1;
}
