// Case 13: the master sends a 4 bit multiframe field in the slot allocation. The real
// TDMA_slave_alloc_exploitation indexes LUT_multif_mask with it (values 0 to 15). The data buffer has the
// exact size, so ASan reports a read behind the buffer, and the lookup table must not be read out of its
// bounds for any of the 16 values (NETWORK_DEBUG_LOG.md 2.36 point 5: the field was 8 bytes long).
#include "mbed.h"

unsigned int CONF_TDMA_slot_duration = 10, CONF_TDMA_slot_margin = 2, offset_time_TX_slave, time_max_TX_burst;
unsigned char my_radio_client_ID = 3;
int slave_alloc_RX_age = 99;
static unsigned char my_multiframe_mask, my_multiframe_ID;
#include "tdma_part.inc"

int main() {
	const unsigned char expected[16] = {0, 1, 3, 7, 15, 31, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};   // 6 to 15 are not defined
	int bad = 0;
	for (int n = 0; n < 16; n++) {
		const int size = 8;                       // [0..1] header, entry for ID 3 in [2..6], end marker [7]
		unsigned char* d = (unsigned char*)malloc(size);
		memset(d, 0, size);
		d[2] = my_radio_client_ID;
		d[3] = 10; d[4] = 0;                      // offset
		d[5] = 4;                                 // slot length
		d[6] = (unsigned char)((n << 4) | 5);     // multiframe mask index n, multiframe ID 5
		d[7] = 0xFF;
		my_multiframe_mask = 0xAA; slave_alloc_RX_age = 99;
		TDMA_slave_alloc_exploitation(d, size);
		if (my_multiframe_mask != expected[n] || my_multiframe_ID != 5 || slave_alloc_RX_age != 0) {
			printf("MISMATCH field %d: mask %d (expected %d), ID %d, age %d\n", n, my_multiframe_mask, expected[n], my_multiframe_ID, slave_alloc_RX_age);
			bad = 1;
		}
		free(d);
	}
	printf("%s\n", bad ? "failed" : "all 16 multiframe fields give the defined mask");
	return bad;
}
