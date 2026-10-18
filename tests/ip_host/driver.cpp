// Host driver for the IP checks: table of cases for the pure functions, then the 'set' commands and 'save'.
#include "HMI_telnet.h"
#include "global_variables.h"
#include "Eth_IPv4.h"
#include "SI4463.h"

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
static char g_out[8192];
static int radio_off_calls, radio_restart_calls, saves;

void HMI_printf_detail(void) { strncat(g_out, HMI_out_str, sizeof(g_out) - strlen(g_out) - 1); }
void HMI_exit(void) {}
int SI4463_set_power(SI4463_Chip*) { return 1; }
void RADIO_off_if_necessary(int) { radio_off_calls++; }
void RADIO_restart_if_necessary(int, int, int) { radio_restart_calls++; }
unsigned int NFPR_config_save(void) { saves++; return 7; }
#include "ip_part.inc"

static int failures = 0;
#define CHECK(cond, ...) do { if (!(cond)) { failures++; printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static unsigned long ip(int a, int b, int c, int d) { return ((unsigned long)a << 24) | (b << 16) | (c << 8) | d; }

static void run_set(const char* name, const char* value) {
	char p1[64], p2[128];
	strcpy(p1, name); strcpy(p2, value);
	g_out[0] = 0; radio_off_calls = radio_restart_calls = 0;
	HMI_set_command(p1, p2);
}
static void run_save(void) { g_out[0] = 0; saves = 0; HMI_save_command(); }

// the network of the master of this project: /28, modem .2, range .3 to .9, route .1 (the Pi)
static void master_config(void) {
	is_TDMA_master = true; CONF_master_FDD = 0; CONF_transmission_method = 0;
	LAN_conf_saved.LAN_modem_IP = ip(192,168,10,2); LAN_conf_saved.LAN_subnet_mask = ip(255,255,255,240);
	CONF_radio_IP_start = ip(192,168,10,3); CONF_radio_IP_size = 7;
	LAN_conf_saved.LAN_def_route = ip(192,168,10,1); LAN_conf_saved.LAN_def_route_activ = 1;
}

int main() {
	// ---- HMI_parse_IP
	struct { const char* s; int ok; unsigned long v; } parse[] = {
		{"1.2.3.4", 1, ip(1,2,3,4)}, {"192.168.10.2", 1, ip(192,168,10,2)}, {"255.255.255.255", 1, 0xFFFFFFFFUL}, {"0.0.0.0", 1, 0},
		{"010.1.1.1", 1, ip(10,1,1,1)}, {"001.002.003.004", 1, ip(1,2,3,4)},
		{"300.1.1.1", 0, 0}, {"256.1.1.1", 0, 0}, {"1.2.3", 0, 0}, {"1.2.3.4.5", 0, 0}, {"1.2.3.4x", 0, 0}, {"1.2.3.4 ", 0, 0},
		{" 1.2.3.4", 0, 0}, {"0x10.1.1.1", 0, 0}, {"1..3.4", 0, 0}, {".1.2.3", 0, 0}, {"1.2.3.", 0, 0}, {"", 0, 0}, {"bad", 0, 0},
		{"-1.2.3.4", 0, 0}, {"1.2.3.0004", 0, 0}, {"1,2,3,4", 0, 0}, {"1.2.3.4\r", 0, 0},
	};
	for (unsigned int i = 0; i < sizeof(parse)/sizeof(parse[0]); i++) {
		unsigned long v = 0x12345678;
		int ok = HMI_parse_IP(parse[i].s, &v);
		CHECK(ok == parse[i].ok, "parse '%s': %d, expected %d", parse[i].s, ok, parse[i].ok);
		if (parse[i].ok && ok) CHECK(v == parse[i].v, "parse '%s': %08lx, expected %08lx", parse[i].s, v, parse[i].v);
		if (!parse[i].ok) CHECK(v == 0x12345678, "parse '%s' changed the result on error", parse[i].s);
	}
	char* longstr = (char*)malloc(301);                    // exact size for ASan
	memset(longstr, '9', 300); longstr[300] = 0;
	unsigned long dummy;
	CHECK(!HMI_parse_IP(longstr, &dummy), "parse of a 300 digit string");
	free(longstr);
	// ---- HMI_netmask_valid: prefix 8 to 30
	struct { unsigned long m; int ok; } masks[] = {
		{ip(255,255,255,240), 1}, {ip(255,255,255,0), 1}, {ip(255,0,0,0), 1}, {ip(255,255,255,252), 1}, {ip(255,255,0,0), 1}, {ip(255,255,255,128), 1},
		{ip(255,255,255,254), 0}, {ip(255,255,255,255), 0}, {ip(254,0,0,0), 0}, {ip(128,0,0,0), 0}, {0, 0},
		{ip(255,255,0,255), 0}, {ip(255,255,255,250), 0}, {ip(0,255,255,255), 0}, {ip(1,2,3,4), 0}, {ip(255,255,255,1), 0},
	};
	for (unsigned int i = 0; i < sizeof(masks)/sizeof(masks[0]); i++)
		CHECK(HMI_netmask_valid(masks[i].m) == masks[i].ok, "netmask %08lx expected %d", masks[i].m, masks[i].ok);
	// ---- HMI_host_IP_valid
	CHECK(HMI_host_IP_valid(ip(1,0,0,1)) && HMI_host_IP_valid(ip(192,168,10,2)) && HMI_host_IP_valid(ip(223,255,255,254)), "valid hosts");
	CHECK(!HMI_host_IP_valid(ip(0,1,2,3)) && !HMI_host_IP_valid(ip(127,0,0,1)) && !HMI_host_IP_valid(ip(224,0,0,1)) && !HMI_host_IP_valid(ip(255,255,255,255)), "invalid hosts");
	// ---- HMI_net_problems
	HMI_net_T n;
	master_config(); HMI_net_from_config(&n);
	CHECK(HMI_net_problems(&n) == 0, "master of the project: %u", HMI_net_problems(&n));
	n.modem_IP = ip(192,168,10,0); CHECK(HMI_net_problems(&n) & HMI_NET_MODEM_IP, "modem_IP = network");
	n.modem_IP = ip(192,168,10,15); CHECK(HMI_net_problems(&n) & HMI_NET_MODEM_IP, "modem_IP = broadcast");
	HMI_net_from_config(&n); n.pool_size = 13; CHECK(HMI_net_problems(&n) & HMI_NET_POOL, "range .3 to .15 reaches the broadcast address");
	HMI_net_from_config(&n); n.pool_size = 14; CHECK(HMI_net_problems(&n) & HMI_NET_POOL, "range beyond the subnet");
	HMI_net_from_config(&n); n.pool_size = 12; CHECK(!(HMI_net_problems(&n) & HMI_NET_POOL), "range .3 to .14 is inside");
	HMI_net_from_config(&n); n.pool_start = ip(192,168,10,0); CHECK(HMI_net_problems(&n) & HMI_NET_POOL, "range starts at the network address");
	HMI_net_from_config(&n); n.pool_start = ip(192,168,11,3); CHECK(HMI_net_problems(&n) & HMI_NET_POOL, "range in another subnet");
	HMI_net_from_config(&n); n.pool_size = 0; CHECK(HMI_net_problems(&n) & HMI_NET_POOL, "range size 0");
	HMI_net_from_config(&n); n.pool_start = ip(255,255,255,250); n.pool_size = 100; CHECK(HMI_net_problems(&n) & HMI_NET_POOL, "range with 32 bit overflow");
	HMI_net_from_config(&n); n.modem_IP = ip(192,168,10,5); CHECK(HMI_net_problems(&n) & HMI_NET_POOL_MODEM, "modem_IP inside the range");
	HMI_net_from_config(&n); n.def_route = ip(10,0,0,1); CHECK(HMI_net_problems(&n) & HMI_NET_ROUTE, "route outside the subnet");
	HMI_net_from_config(&n); n.def_route = ip(192,168,10,2); CHECK(!(HMI_net_problems(&n) & HMI_NET_ROUTE), "route = modem_IP (the default configuration)");
	HMI_net_from_config(&n); n.check_route = 0; n.def_route = ip(10,0,0,1); CHECK(HMI_net_problems(&n) == 0, "route not active");
	HMI_net_from_config(&n); n.check_pool = 0; n.pool_start = 0; n.pool_size = 0; CHECK(HMI_net_problems(&n) == 0, "client: range not checked");
	// the default configuration of config_flash.h as master and as client
	LAN_conf_saved.LAN_modem_IP = ip(192,168,0,253); LAN_conf_saved.LAN_subnet_mask = ip(255,255,255,0);
	CONF_radio_IP_start = ip(192,168,0,65); CONF_radio_IP_size = 32; LAN_conf_saved.LAN_def_route = ip(192,168,0,253); LAN_conf_saved.LAN_def_route_activ = 1;
	is_TDMA_master = true; HMI_net_from_config(&n); CHECK(HMI_net_problems(&n) == 0, "default configuration as master");
	is_TDMA_master = false; HMI_net_from_config(&n); CHECK(HMI_net_problems(&n) == 0 && !n.check_pool, "default configuration as client");
	master_config(); CONF_master_FDD = 2; HMI_net_from_config(&n); CHECK(!n.check_pool, "master FDD up: range not checked");
	master_config(); CONF_transmission_method = 1; HMI_net_from_config(&n); CHECK(!n.check_pool, "transmission method 1: range not checked");
	// ---- 'set' commands
	master_config();
	run_set("netmask", "255.255.0.255");
	CHECK(strstr(g_out, "bad netmask") && LAN_conf_saved.LAN_subnet_mask == ip(255,255,255,240) && radio_off_calls == 0, "set netmask 255.255.0.255: '%s'", g_out);
	run_set("netmask", "255.255.255.255");
	CHECK(strstr(g_out, "bad netmask") && LAN_conf_saved.LAN_subnet_mask == ip(255,255,255,240), "set netmask /32");
	run_set("netmask", "300.1.1.1");
	CHECK(strstr(g_out, "bad IP format") && LAN_conf_saved.LAN_subnet_mask == ip(255,255,255,240), "set netmask 300.1.1.1");
	run_set("netmask", "255.255.255.0");
	CHECK(strstr(g_out, "OK") && !strstr(g_out, "warning") && LAN_conf_saved.LAN_subnet_mask == ip(255,255,255,0) && radio_off_calls == 1 && radio_restart_calls == 1, "set netmask /24: '%s'", g_out);
	run_set("netmask", "255.255.255.240");
	run_set("modem_IP", "192.168.10.0");
	CHECK(strstr(g_out, "warning: modem_IP is the network") && strstr(g_out, "OK") && LAN_conf_saved.LAN_modem_IP == ip(192,168,10,0), "set modem_IP network address: '%s'", g_out);
	run_set("modem_IP", "192.168.10.2");
	CHECK(!strstr(g_out, "warning") && LAN_conf_saved.LAN_modem_IP == ip(192,168,10,2), "set modem_IP back: '%s'", g_out);
	run_set("modem_IP", "0.0.0.0");
	CHECK(strstr(g_out, "0.0.0.0 is not allowed") && !strstr(g_out, "OK") && LAN_conf_saved.LAN_modem_IP == ip(192,168,10,2), "set modem_IP 0.0.0.0: '%s'", g_out);
	run_set("modem_IP", "127.0.0.1");
	CHECK(strstr(g_out, "not a host address") && LAN_conf_saved.LAN_modem_IP == ip(192,168,10,2), "set modem_IP 127.0.0.1");
	run_set("modem_IP", "224.0.0.5");
	CHECK(strstr(g_out, "not a host address"), "set modem_IP 224.0.0.5");
	run_set("modem_IP", "1.2.3");
	CHECK(strstr(g_out, "bad IP format") && radio_off_calls == 0, "set modem_IP 1.2.3");
	run_set("modem_IP", "192.168.10.04");
	CHECK(strstr(g_out, "OK") && LAN_conf_saved.LAN_modem_IP == ip(192,168,10,4) && strstr(g_out, "warning: modem_IP is inside"), "set modem_IP 192.168.10.04 (decimal, in the range): '%s'", g_out);
	run_set("modem_IP", "192.168.10.2");
	run_set("IP_begin", "10.0.0.5");
	CHECK(strstr(g_out, "warning: the IP range") && strstr(g_out, "OK") && CONF_radio_IP_start == ip(10,0,0,5), "set IP_begin outside the subnet: '%s'", g_out);
	run_set("IP_begin", "192.168.10.3");
	run_set("master_IP_size", "20");
	CHECK(strstr(g_out, "warning: the IP range") && CONF_radio_IP_size == 20, "set master_IP_size 20: '%s'", g_out);
	run_set("master_IP_size", "7");
	CHECK(!strstr(g_out, "warning") && CONF_radio_IP_size == 7, "set master_IP_size 7");
	run_set("def_route_val", "10.9.9.9");
	CHECK(strstr(g_out, "warning: def_route_val") && LAN_conf_saved.LAN_def_route == ip(10,9,9,9), "set def_route_val outside: '%s'", g_out);
	run_set("def_route_val", "192.168.10.1");
	run_set("DNS_value", "8.8.8.8");
	CHECK(!strstr(g_out, "warning") && strstr(g_out, "OK") && LAN_conf_saved.LAN_DNS_value == ip(8,8,8,8), "set DNS_value: '%s'", g_out);
	run_set("DNS_value", "255.1.1.1");
	CHECK(strstr(g_out, "not a host address") && LAN_conf_saved.LAN_DNS_value == ip(8,8,8,8), "set DNS_value 255.1.1.1");
	run_set("master_down_IP", "300.0.0.1");
	CHECK(strstr(g_out, "bad IP format"), "set master_down_IP 300.0.0.1");
	// ---- 'save'
	master_config(); run_save();
	CHECK(saves == 1 && strstr(g_out, "Configuration saved"), "save with the master of the project: '%s'", g_out);
	LAN_conf_saved.LAN_modem_IP = ip(192,168,10,0); run_save();
	CHECK(saves == 0 && strstr(g_out, "cannot save") && strstr(g_out, "  - modem_IP is the network") && strstr(g_out, "ready> "), "save with modem_IP = network: '%s'", g_out);
	master_config(); CONF_radio_IP_size = 30; run_save();
	CHECK(saves == 0 && strstr(g_out, "  - the IP range"), "save with a range beyond the subnet: '%s'", g_out);
	master_config(); is_TDMA_master = false; CONF_radio_IP_size = 30; run_save();
	CHECK(saves == 1, "client: the range is not checked, save works");
	master_config(); LAN_conf_saved.LAN_subnet_mask = ip(255,255,255,0); LAN_conf_saved.LAN_def_route = ip(172,16,0,1); run_save();
	CHECK(saves == 0 && strstr(g_out, "  - def_route_val"), "save with a route outside the subnet: '%s'", g_out);
	printf("%s\n", failures ? "failed" : "all cases passed");
	return failures ? 1 : 0;
}
