// Case 2: segments of one IPv4 packet from the radio. Master, client 6 (last row of ethernet_buffer),
// 8 segments of 252 bytes, each with the continuation counter in order. The firmware's own sender never
// sends more than 6 segments. ASan reports the write behind ethernet_buffer.
#include "mbed.h"
#include "reasm_stub.h"

Timer GLOBAL_timer;
unsigned char RX_FIFO_data[0x2000];
unsigned int RX_FIFO_RD_point, RX_FIFO_last_received, G_rx_seg_gap;
bool is_TDMA_master;
unsigned long int last_rframe_seen, CONF_radio_timeout = 1000000;
char CONF_radio_addr_table_status[radio_addr_table_size];
unsigned char my_radio_client_ID;
int G_downlink_bandwidth_temp, RX_radio_IPv4_counter, super_debug;
int argc_segments, from_radio_calls, from_radio_size;
short int TDMA_TA_measure_single_frame(unsigned int, unsigned char, unsigned char, int) { return 0; }
void SI4463_prepa_TX_1(void) {}
void radio_save_RSSI_BER(unsigned char, unsigned char, unsigned char, unsigned int) {}
void radio_count_fec(unsigned char, unsigned char, unsigned int) {}
void IPv4_from_radio(unsigned char*, int size) { from_radio_calls++; from_radio_size = size; }
void signaling_frame_exploitation(unsigned char*, int, int) {}
void TDMA_slave_alloc_exploitation(unsigned char*, int) {}
void TDMA_byte_RX_interp(unsigned char, unsigned char, unsigned char, unsigned int) {}
static unsigned char data_RX[360];
#include "L1L2_part.inc"

int main(int argc, char** argv) {
	int nseg = argc > 1 ? atoi(argv[1]) : 8;
	is_TDMA_master = 1;
	CONF_radio_addr_table_status[6] = 1;
	for (int seg = 0; seg < nseg; seg++) {
		unsigned char payload[255];
		memset(payload, 0x55, sizeof(payload));
		payload[0] = 6;                      // client address 6, parity not checked after FEC
		payload[1] = 0x02;                   // protocol IPv4
		payload[2] = (7 & 0x0F) * 0x10 + (seg == nseg - 1 ? 0x08 : 0) + (seg & 0x07); // packet 7, last flag, counter
		unsigned char frame[400];
		memset(frame, 0, sizeof(frame));
		int size_w_fec = FEC_encode2(payload, frame + 6, 255);   // 344
		RX_FIFO_RD_point = 0;
		RX_FIFO_last_received = 6 + size_w_fec;
		frame[3] = 100;                      // RSSI
		frame[4] = size_w_fec + 1 - SI4463_offset_size;
		frame[5] = 0x00;                     // TDMA byte: uplink
		memcpy(RX_FIFO_data, frame, 6 + size_w_fec);
		radio_RX_FIFO_dequeue(0);
	}
	printf("%d segments handled, IPv4_from_radio calls %d, last size %d\n", nseg, from_radio_calls, from_radio_size);
	return 0;
}
