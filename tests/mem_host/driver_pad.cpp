// Case 10a: segment_and_push with a packet shorter than the 63 byte minimum of a radio segment.
// The real function runs with a heap buffer of exactly the packet size: ASan reports a read behind it.
// FEC_encode2 is replaced by a stub that keeps its input, so the padding bytes can be checked.
#include "mbed.h"
#define SI4463_offset_size 90

Timer GLOBAL_timer;
unsigned char parity_bit_elab[128];
int G_uplink_bandwidth_temp, TX_radio_IPv4_counter;
static unsigned char rframe_TX[384];
static unsigned char captured[300];
static int captured_size;
int TX_FIFO_full_global(int) { return 0; }
void TX_FIFO_write_global(unsigned char*, int) {}
int FEC_encode2(unsigned char* data_in, unsigned char* data_out, int size_in) {
	captured_size = size_in;
	memcpy(captured, data_in, size_in);
	int n = (size_in + 2) / 3;
	memset(data_out, 0, 4 * (n + 1));
	return 4 * (n + 1);
}
#include "pad_part.inc"

int main() {
	int failures = 0;
	int sizes[] = {0, 1, 20, 62, 63, 64, 100, 252, 253, 300};
	for (unsigned int k = 0; k < sizeof(sizes)/sizeof(sizes[0]); k++) {
		int n = sizes[k];
		unsigned char* buf = (unsigned char*)malloc(n > 0 ? n : 1);   // exactly n bytes, ASan watches the end
		memset(buf, 'X', n);
		int first_segment = n > 252 ? 252 : n;
		captured_size = 0;
		segment_and_push(buf, n, 1, 0x02);
		// check the first segment: header (3), packet bytes, then zeros up to 63
		int bad = 0;
		for (int i = 0; i < first_segment; i++) if (captured[3 + i] != 'X') bad = 1;
		for (int i = first_segment; i < 63; i++) if (captured_size == 63 + 3 && captured[3 + i] != 0) bad = 1;
		if (n > 252) { /* second segment overwrote the capture, checked below */ }
		printf("size %d: segment of %d bytes, %s\n", n, captured_size, bad ? "BAD PADDING" : "padding ok");
		if (n <= 252 && bad) failures++;
		if (n > 252) { // last segment holds n-252 bytes, padded to 63
			int rest = n - 252, last_bad = 0;
			for (int i = 0; i < rest; i++) if (captured[3 + i] != 'X') last_bad = 1;
			for (int i = rest; i < 63; i++) if (captured[3 + i] != 0) last_bad = 1;
			if (last_bad) { failures++; printf("size %d: last segment BAD PADDING\n", n); }
		}
		free(buf);
	}
	return failures ? 1 : 0;
}
