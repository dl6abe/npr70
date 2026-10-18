// Host stub: the globals and callbacks that radio_RX_FIFO_dequeue and FEC_decode use.
#ifndef REASM_STUB_H
#define REASM_STUB_H
#include "mbed.h"
#define RX_FIFO_mask 0x1FFF
#define SI4463_offset_size 90
#define SI4463_CONF_max_field2_size 345
#define radio_addr_table_size 7
struct W5500_chip;
extern Timer GLOBAL_timer;
extern unsigned char RX_FIFO_data[0x2000];
extern unsigned int RX_FIFO_RD_point, RX_FIFO_last_received;
extern bool is_TDMA_master;
extern unsigned long int last_rframe_seen, CONF_radio_timeout;
extern char CONF_radio_addr_table_status[radio_addr_table_size];
extern unsigned char my_radio_client_ID;
extern unsigned int G_rx_seg_gap;
extern int G_downlink_bandwidth_temp, RX_radio_IPv4_counter, super_debug;
extern int argc_segments, from_radio_calls, from_radio_size;
short int TDMA_TA_measure_single_frame(unsigned int, unsigned char, unsigned char, int);
void SI4463_prepa_TX_1(void);
void radio_save_RSSI_BER(unsigned char, unsigned char, unsigned char, unsigned int);
void radio_count_fec(unsigned char, unsigned char, unsigned int);
void IPv4_from_radio(unsigned char*, int);
void signaling_frame_exploitation(unsigned char*, int, int);
void TDMA_slave_alloc_exploitation(unsigned char*, int);
void TDMA_byte_RX_interp(unsigned char, unsigned char, unsigned char, unsigned int);
int FEC_encode2(unsigned char*, unsigned char*, int);
int FEC_decode(unsigned char*, int, unsigned int*);
void radio_RX_FIFO_dequeue(W5500_chip*);
#endif
