// Host stub: the settings that 'set' writes.
#ifndef STUB_GLOBAL_VARIABLES_H
#define STUB_GLOBAL_VARIABLES_H
#include "mbed.h"
#define FREQ_RANGE_MIN 420
#define FREQ_RANGE_MAX 450
struct LAN_conf_T {
	unsigned long int LAN_modem_IP;
	unsigned long int DHCP_range_start;
	unsigned long int DHCP_range_size;
	unsigned long int LAN_subnet_mask;
	unsigned long int LAN_def_route;
	unsigned char LAN_def_route_activ;
	unsigned char LAN_DNS_activ;
	unsigned long int LAN_DNS_value;
	unsigned char DHCP_server_active;
};
extern LAN_conf_T LAN_conf_saved;
extern bool is_TDMA_master;
extern unsigned char is_telnet_active;
extern unsigned char is_telnet_routed;
extern unsigned char CONF_radio_modulation;
extern unsigned char CONF_radio_network_ID;
extern unsigned short int CONF_frequency_HD;
extern short int CONF_freq_shift;
extern unsigned char CONF_master_FDD;
extern unsigned char CONF_Eth_mode;
extern unsigned long int CONF_master_down_IP;
extern unsigned long int CONF_radio_IP_start;
extern unsigned long int CONF_radio_IP_size;
extern unsigned long int CONF_radio_IP_size_requested;
extern unsigned char CONF_radio_PA_PWR;
extern unsigned char CONF_radio_default_state_ON_OFF;
extern char CONF_radio_my_callsign[16];
extern unsigned char CONF_modem_MAC[6];
extern int is_telnet_opened;
struct SI4463_Chip;
extern SI4463_Chip* G_SI4463;
int SI4463_set_power(SI4463_Chip* SI4463);
void RADIO_off_if_necessary(int need_disconnect);
void RADIO_restart_if_necessary(int need_disconnect, int need_radio_reconfigure, int HMI_output);
#endif
