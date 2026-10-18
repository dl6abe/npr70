// Host stub: the globals HMI_print_status reads. HMI_out_str has the size of the firmware's buffer.
#ifndef STATUS_STUB_H
#define STATUS_STUB_H
#include "mbed.h"
#define radio_addr_table_size 7
extern char HMI_out_str[120];
void HMI_printf_detail(void);
#define HMI_printf(param, ...) sprintf (HMI_out_str, param, ##__VA_ARGS__);\
	HMI_printf_detail();
extern int slow_counter;
extern bool is_TDMA_master;
extern int G_uplink_bandwidth_temp, G_downlink_bandwidth_temp, G_temperature_SI4463;
extern long int TDMA_table_TA[radio_addr_table_size];
extern unsigned char my_radio_client_ID, CONF_radio_state_ON_OFF, G_connect_rejection_reason, CONF_master_FDD;
extern unsigned int my_client_radio_connexion_state, RX_top_FDD_up_counter, RSSI_total_stat, RSSI_stat_pkt_nb;
extern unsigned int G_rx_fec_ok, G_rx_fec_fixed, G_rx_fec_lost, G_rx_seg_gap;
extern int RX_Eth_IPv4_counter, TX_radio_IPv4_counter, RX_radio_IPv4_counter;
extern unsigned short int G_downlink_RSSI, G_downlink_BER;
extern unsigned short int G_radio_addr_table_RSSI[radio_addr_table_size], G_radio_addr_table_BER[radio_addr_table_size];
void HMI_print_status(void);
#endif
