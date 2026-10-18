// Host driver for the DHCP code: feeds random DHCP packets and direct lookfor_free_LAN_IP calls
// from a fixed seed and prints every answer. run.sh compares the output of two builds.
#include "mbed.h"
#include "global_variables.h"
#include "W5500.h"
#include "Eth_IPv4.h"

int lookfor_free_LAN_IP (LAN_conf_T* LAN_config, unsigned char* client_MAC, unsigned char* requested_IP, unsigned char* proposed_IP, int req_type);
void DHCP_server(LAN_conf_T* LAN_config, W5500_chip* W5500);
void reset_DHCP_table(LAN_conf_T* LAN_config);

Timer GLOBAL_timer;
#include "ip_conv.inc"

static unsigned char pending[600];
static int pending_size = 0;

uint16_t W5500_read_received_size(W5500_chip*, uint8_t) { return pending_size; }
uint32_t W5500_read_UDP_pckt(W5500_chip*, uint8_t, unsigned char* data, uint32_t) {
	memcpy(data, pending, sizeof(pending));
	int s = pending_size;
	pending_size = 0;
	return s;
}
void W5500_write_TX_buffer(W5500_chip*, uint8_t, unsigned char* data, int size, int) {
	printf("TX %d:", size);
	for (int i = 0; i < size; i++) printf(" %02x", data[i]);
	printf("\n");
}

static unsigned int rng_state = 12345;
static unsigned int rnd(unsigned int n) { rng_state = rng_state * 1103515245u + 12345u; return ((rng_state >> 8) & 0xFFFFFF) % n; }

int main(int argc, char** argv) {
	int rounds = argc > 1 ? atoi(argv[1]) : 20000;
	W5500_chip chip;
	for (int cfg = 0; cfg < 8; cfg++) {
		LAN_conf_T lan;
		memset(&lan, 0, sizeof(lan));
		lan.LAN_modem_IP = 0xC0A80A02;
		lan.DHCP_range_start = 0xC0A80A03;
		lan.DHCP_range_size = (cfg % 3 == 0) ? 7 : (cfg % 3 == 1 ? 3 : 40);
		lan.LAN_subnet_mask = 0xFFFFFFF0;
		lan.LAN_def_route = 0xC0A80A01;
		lan.LAN_def_route_activ = cfg & 1;
		lan.LAN_DNS_activ = (cfg >> 1) & 1;
		lan.LAN_DNS_value = 0x08080808;
		reset_DHCP_table(&lan);
		printf("CONFIG %d\n", cfg);
		for (int n = 0; n < rounds; n++) {
			unsigned char mac[6];
			int client = rnd(6);
			for (int i = 0; i < 6; i++) mac[i] = 0x10 * client + i;
			int type = (int[]){1, 3, 1, 3, 7, 2, 8, 0}[rnd(8)];
			unsigned long int ip = lan.DHCP_range_start - 2 + rnd(lan.DHCP_range_size + 4);
			unsigned char req_ip[4];
			IP_int2char(ip, req_ip);
			if (rnd(3) == 0) {
				// direct call
				unsigned char proposed[4] = {0, 0, 0, 0};
				int a = lookfor_free_LAN_IP(&lan, mac, req_ip, proposed, type);
				printf("L %d %d %u.%u.%u.%u\n", type, a, proposed[0], proposed[1], proposed[2], proposed[3]);
			} else {
				memset(pending, 0, sizeof(pending));
				pending[8] = rnd(10) ? 1 : 2;
				for (int i = 0; i < 4; i++) pending[12 + i] = rnd(256);
				if (rnd(4) == 0) for (int i = 0; i < 4; i++) pending[20 + i] = req_ip[i];
				for (int i = 0; i < 6; i++) pending[36 + i] = mac[i];
				int p = 248;
				if (rnd(8)) { pending[p++] = 53; pending[p++] = 1; pending[p++] = type; }
				if (rnd(2)) { pending[p++] = 50; pending[p++] = 4; for (int i = 0; i < 4; i++) pending[p++] = req_ip[i]; }
				if (rnd(4) == 0) { pending[p++] = 54; pending[p++] = 4; for (int i = 0; i < 4; i++) pending[p++] = 1; }
				pending[p++] = 255;
				pending_size = p + rnd(3);
				printf("S %d\n", type);
				DHCP_server(&lan, &chip);
			}
		}
	}
	return 0;
}
