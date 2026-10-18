// Host driver for the 'set' command: runs a list of parameter/value pairs and prints the answer,
// the radio calls and all settings after each one. run.sh compares the output of two builds.
#include "HMI_telnet.h"
#include "global_variables.h"
#include "Eth_IPv4.h"

LAN_conf_T LAN_conf_saved;
bool is_TDMA_master;
unsigned char is_telnet_active, is_telnet_routed, CONF_radio_modulation, CONF_radio_network_ID, CONF_master_FDD, CONF_transmission_method, CONF_Eth_mode, CONF_radio_PA_PWR, CONF_radio_default_state_ON_OFF;
unsigned short int CONF_frequency_HD;
short int CONF_freq_shift;
unsigned long int CONF_master_down_IP, CONF_radio_IP_start, CONF_radio_IP_size, CONF_radio_IP_size_requested;
char CONF_radio_my_callsign[16];
unsigned char CONF_modem_MAC[6] = {1, 2, 3, 4, 5, 6};
int is_telnet_opened;
SI4463_Chip* G_SI4463;
char HMI_out_str[1024];
#include "ip_conv.inc"

void HMI_printf_detail(void) { printf("OUT[%s]\n", HMI_out_str); }
void HMI_exit(void) { printf("CALL HMI_exit\n"); is_telnet_opened = 0; }
int SI4463_set_power(SI4463_Chip*) { printf("CALL set_power %u\n", CONF_radio_PA_PWR); return 1; }
void RADIO_off_if_necessary(int a) { printf("CALL off %d\n", a); }
void RADIO_restart_if_necessary(int a, int b, int c) { printf("CALL restart %d %d %d\n", a, b, c); }

static void dump(void) {
	printf("STATE %d %d %d %u %u %u %u %ld %u %u %u %u %u %lx %lx %lx %lx %lx %lx %lx %lx %lx %lx %lx %lx %d [%s] %d\n",
		is_TDMA_master, is_telnet_active, is_telnet_routed, CONF_radio_modulation, CONF_radio_network_ID, CONF_frequency_HD,
		(unsigned)(unsigned short)CONF_freq_shift, (long)CONF_freq_shift, CONF_master_FDD, CONF_Eth_mode, CONF_radio_PA_PWR,
		CONF_radio_default_state_ON_OFF, LAN_conf_saved.LAN_DNS_activ, CONF_master_down_IP, CONF_radio_IP_start, CONF_radio_IP_size,
		CONF_radio_IP_size_requested, LAN_conf_saved.LAN_modem_IP, LAN_conf_saved.LAN_subnet_mask, LAN_conf_saved.LAN_def_route,
		LAN_conf_saved.LAN_DNS_value, (unsigned long)LAN_conf_saved.LAN_def_route_activ, (unsigned long)LAN_conf_saved.DHCP_server_active,
		0UL, 0UL, is_telnet_opened, CONF_radio_my_callsign + 2, CONF_radio_my_callsign[0] + CONF_radio_my_callsign[1]);
}

// The IP rules are stricter than the frozen original (tests/ip_host checks them). The original accepts these
// pairs and sets a wrong or meaningless value, so a differential test makes no sense for them.
static int differs_by_design(const char* name, const char* value) {
	const char* ip_names[] = {"modem_IP", "netmask", "def_route_val", "DNS_value", "IP_begin", "master_down_IP"};
	int is_ip = 0;
	for (unsigned int i = 0; i < sizeof(ip_names)/sizeof(ip_names[0]); i++) {
		if (strcmp(name, ip_names[i]) == 0) is_ip = 1;
	}
	if (!is_ip) return 0;
	if (strcmp(value, "0.0.0.0") == 0 || strcmp(value, "300.1.1.1") == 0 || strcmp(value, "bad") == 0) return 1;
	if (strcmp(name, "netmask") == 0) return strcmp(value, "255.255.255.240") != 0;     // only a contiguous mask is valid
	return strcmp(value, "255.255.255.240") == 0;                                        // 255.x.x.x is no host address
}

int main() {
	const char* names[] = {"callsign", "is_master", "telnet_active", "telnet_routed", "DNS_active", "def_route_active", "master_FDD",
		"radio_on_at_start", "DHCP_active", "modem_IP", "netmask", "def_route_val", "DNS_value", "IP_begin", "master_down_IP",
		"master_IP_size", "client_req_size", "frequency", "freq_shift", "RF_power", "Eth_mode", "modulation", "radio_netw_ID",
		"unknown", "trans_method", "Callsign", ""};
	const char* values[] = {"yes", "no", "maybe", "no", "yes", "up", "down", "1.2.3.4", "192.168.10.2", "bad", "0.0.0.0", "300.1.1.1",
		"5", "0", "7", "6", "127", "128", "-3", "12", "16", "21", "24", "10", "x", "440.5", "439.7", "445", "419", "-4.8", "4.8", "11",
		"DL6ABE-70", "ABCDEFGHIJKLM", "ABCDEFGHIJKLMN", "-1", "300", "255.255.255.240"};
	unsigned int n_names = sizeof(names)/sizeof(names[0]), n_values = sizeof(values)/sizeof(values[0]);
	for (unsigned int i = 0; i < n_names; i++) {
		for (unsigned int j = 0; j < n_values; j++) {
			for (int master = 0; master < 2; master++) {
				for (int dhcp = 0; dhcp < 2; dhcp++) {
					is_TDMA_master = master;
					LAN_conf_saved.DHCP_server_active = dhcp;
					is_telnet_opened = (i + j) & 1;
					char p1[64], p2[64];
					strcpy(p1, names[i]);
					strcpy(p2, values[j]);
					if (differs_by_design(p1, p2)) continue;
					printf("SET %s %s m%d d%d\n", p1, p2, master, dhcp);
					HMI_set_command(p1, p2);
					dump();
				}
			}
		}
	}
	char p1[] = "RF_power";
	printf("NULL\n");
	HMI_set_command(p1, NULL);
	HMI_set_command(NULL, p1);
	HMI_set_command(NULL, NULL);
	return 0;
}
