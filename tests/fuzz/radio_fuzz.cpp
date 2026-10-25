// libFuzzer target for the radio receive path: radio_RX_FIFO_dequeue with FEC_decode and the reassembly of segments
// (L1L2_radio.cpp, cut out by tools/fuzz.sh like in mem_host).
// Byte 0: bit 0 = 1 raw mode, bit 1 = master, bits 2 to 4 = own client ID.
// Byte 1: bit mask of the clients known to the master (CONF_radio_addr_table_status).
// Then records of [length byte][bytes]. All records go to one receiver, so the reassembly state carries over.
//   structured (bit 0 = 0): bytes = TDMA byte, change of the frame length field, RSSI, then the payload that is
//     FEC encoded here, so the fuzzer reaches the protocol code behind a valid FEC (a random frame never does).
//   raw (bit 0 = 1): bytes are the frame as the radio driver stores it in the RX FIFO (6 byte header, FEC data).
#include "mbed.h"
#include "reasm_stub.h"

Timer GLOBAL_timer;
unsigned char RX_FIFO_data[0x2000];
unsigned int RX_FIFO_RD_point, RX_FIFO_last_received, G_rx_seg_gap;
unsigned int G_buf_max[DIAG_BUF_N], G_buf_eth_rows;
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
static volatile unsigned char sink;
static void read_all(unsigned char* d, int n) { for (int i = 0; i < n; i++) sink ^= d[i]; }   // ASan checks the length
void IPv4_from_radio(unsigned char* d, int size) { from_radio_calls++; from_radio_size = size; read_all(d, size); }
void signaling_frame_exploitation(unsigned char* d, int size, int) { read_all(d, size); }
void TDMA_slave_alloc_exploitation(unsigned char* d, int size) { read_all(d, size); }
void TDMA_byte_RX_interp(unsigned char, unsigned char, unsigned char, unsigned int) {}
static unsigned char data_RX[360];
#include "L1L2_part.inc"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size < 2) return 0;
    bool raw = data[0] & 1;
    is_TDMA_master = (data[0] >> 1) & 1;
    my_radio_client_ID = (data[0] >> 2) & 7;
    for (int i = 0; i < radio_addr_table_size; i++) CONF_radio_addr_table_status[i] = (data[1] >> i) & 1;
    size_t i = 2;
    while (i < size) {
        size_t n = data[i++];
        if (n > size - i) n = size - i;
        const uint8_t *rec = data + i;
        i += n;
        unsigned char frame[400];
        memset(frame, 0, sizeof(frame));
        int frame_size;
        if (raw) {
            if (n > sizeof(frame)) n = sizeof(frame);
            memcpy(frame, rec, n);
            frame_size = (int)n;
        } else {
            if (n < 3) continue;
            unsigned char payload[255];
            int plen = (int)n - 3;
            if (plen > (int)sizeof(payload)) plen = sizeof(payload);
            memcpy(payload, rec + 3, plen);
            int size_w_fec = FEC_encode2(payload, frame + 6, plen);
            frame[3] = rec[2];                                                      // RSSI
            frame[4] = (unsigned char)(size_w_fec + 1 - SI4463_offset_size + (signed char)rec[1]);
            frame[5] = rec[0];                                                      // TDMA byte
            frame_size = 6 + size_w_fec;
        }
        memcpy(RX_FIFO_data, frame, frame_size);
        RX_FIFO_RD_point = 0;
        RX_FIFO_last_received = frame_size;
        radio_RX_FIFO_dequeue(0);
    }
    return 0;
}
