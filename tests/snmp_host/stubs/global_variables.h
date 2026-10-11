// Host stub: only the globals SNMP.cpp reads.
#ifndef STUB_GLOBAL_VARIABLES_H
#define STUB_GLOBAL_VARIABLES_H
#define radio_addr_table_size 7
extern bool is_TDMA_master;
extern unsigned char my_radio_client_ID;
extern unsigned int my_client_radio_connexion_state;
extern int G_temperature_SI4463;
extern long int TDMA_table_TA[radio_addr_table_size];
extern unsigned short int G_radio_addr_table_RSSI[radio_addr_table_size];
extern unsigned short int G_radio_addr_table_BER[radio_addr_table_size];
extern unsigned short int G_downlink_RSSI;
extern unsigned short int G_downlink_BER;
extern char CONF_radio_addr_table_status[radio_addr_table_size];
extern char CONF_radio_addr_table_callsign[radio_addr_table_size][16];
extern unsigned char G_reset_reason;
extern char CONF_radio_my_callsign[16];
#endif
