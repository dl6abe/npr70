// Case 12: the master answers a connection request of a connected client with a NACK. The real
// signaling_connect_NACK_process must put the client back to the unconnected address 0x7E. The master
// gives the old ID no TX slot any more, so a client that keeps it never reaches the master again.
#include "mbed.h"

unsigned char G_connect_rejection_reason;
unsigned char my_radio_client_ID;
int my_client_radio_connexion_state, connect_state_machine_counter;
static int null_frame_size = -1;
void TDMA_NULL_frame_init(int size) { null_frame_size = size; }
#include "nack_part.inc"

int main() {
	my_radio_client_ID = 3;
	my_client_radio_connexion_state = 2;
	connect_state_machine_counter = 9;
	signaling_connect_NACK_process(2);
	printf("after NACK: state %d, reason %d, own ID 0x%02X, counter %d, NULL frame size %d\n",
		my_client_radio_connexion_state, G_connect_rejection_reason, my_radio_client_ID,
		connect_state_machine_counter, null_frame_size);
	return (my_client_radio_connexion_state == 3 && G_connect_rejection_reason == 2
		&& my_radio_client_ID == 0x7E && connect_state_machine_counter == 0 && null_frame_size == 70) ? 0 : 1;
}
