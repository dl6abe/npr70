// Case 15: TA measurement of the master in the path without CONF_master_FDD == 1 (issue #48). frame_timer is the
// 24 bit timer of the radio chip, the master top stamp is masked to 24 bits. When the timer wraps between the top
// stamp and the frame, the difference must still be the small positive TA and not -2^24 + TA.
#include "mbed.h"

#define radio_addr_table_size 7
enum { DIAG_TA_FRAME, DIAG_TA_SYNC_UP, DIAG_TA_PARITY, DIAG_TA_UNKNOWN, DIAG_TA_UNCONNECTED, DIAG_TA_ACCEPTED, DIAG_TA_BELOW, DIAG_TA_ABOVE, DIAG_TA_N };
static int ta_n[DIAG_TA_N];
static int ta_last_raw;
static void DIAG_ta_count(int what) { ta_n[what]++; }
static void DIAG_ta_value(int what, int a, int) { ta_n[what]++; ta_last_raw = a; }
unsigned char parity_bit_check[256];
unsigned char CONF_master_FDD = 0;
unsigned int TDMA_slave_last_master_top = 0;
char CONF_radio_addr_table_status[radio_addr_table_size];
long int TDMA_table_offset[radio_addr_table_size];
long int TDMA_table_TA[radio_addr_table_size];
unsigned char G_ta_valid[radio_addr_table_size];
int TDMA_offset_multi_frame = 0;
#include "ta_part.inc"

int main() {
	memset(parity_bit_check, 1, sizeof parity_bit_check);
	CONF_radio_addr_table_status[2] = 1;
	// top stamp (any width, the code masks it), frame timer (24 bit), expected TA; client 2, uplink frame with the synchro bit, 114 bytes
	struct { unsigned int top, frame; int ta; const char* what; } k[] = {
		{ 1000u, 1161u, 161, "no wrap" },
		{ 0x00FFFFF0u, 0x000000A1u, 177, "frame timer wrapped after the top stamp" },
		{ 0xFFFFFFF0u, 0x000000A1u, 177, "top stamp with bits above 24, wrapped" },
		{ 0x01000010u, 0x000001A1u, 401, "top stamp with bits above 24, no wrap" },
		{ 0x00000010u, 0x0000000Fu, -1, "frame slightly before the top stamp, no wrap" },
	};
	int bad = 0;
	for (unsigned i = 0; i < sizeof k / sizeof k[0]; i++) {
		memset(ta_n, 0, sizeof ta_n);
		TDMA_slave_last_master_top = k[i].top;
		G_ta_valid[2] = 0;
		TDMA_TA_measure_single_frame(k[i].frame, 0x20 | 0x00, 2, 114);
		// the return value is cut to 16 bits and hides the wrap, so check the raw value that the window sees
		if (!G_ta_valid[2] || ta_n[DIAG_TA_ACCEPTED] != 1 || ta_last_raw != k[i].ta) {
			printf("MISMATCH %s: raw TA %d (expected %d), accepted %d, below %d\n", k[i].what, ta_last_raw, k[i].ta, ta_n[DIAG_TA_ACCEPTED], ta_n[DIAG_TA_BELOW]);
			bad = 1;
		}
	}
	// Return value of an unconnected client (0x7E, connect request): the caller hands it to TDMA_init_TA, whose window is
	// -200 to 2000 us. A value of 65536 + 500 must not be cut to 16 bits and fall into that window (issue #49).
	struct { unsigned int top, frame; int ta; const char* what; } r[] = {
		{ 0u, 66036u, 66036, "65536 + 500 us" },
		{ 0u, 40000u, 40000, "40000 us (above 32767)" },
		{ 1000u, 1161u, 161, "small value" },
	};
	for (unsigned i = 0; i < sizeof r / sizeof r[0]; i++) {
		TDMA_slave_last_master_top = r[i].top;
		int got = TDMA_TA_measure_single_frame(r[i].frame, 0x20 | 0x00, 0x7E, 114);
		if (got != r[i].ta) {
			printf("MISMATCH return value %s: %d (expected %d)\n", r[i].what, got, r[i].ta);
			bad = 1;
		}
	}
	printf("%s\n", bad ? "failed" : "TA difference correct across the 24 bit wrap, return value not cut");
	return bad;
}
