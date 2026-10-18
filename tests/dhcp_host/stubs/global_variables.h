// Host stub: LAN configuration and timer, the only globals the DHCP code reads.
#ifndef STUB_GLOBAL_VARIABLES_H
#define STUB_GLOBAL_VARIABLES_H
#include "mbed.h"
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
extern Timer GLOBAL_timer;
#endif
