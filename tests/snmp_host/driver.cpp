// Host driver: reads a hex SNMP request from stdin, runs the firmware parser, prints the hex response.
// Optional argv: "master" or "client" selects the global state.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "SNMP.h"

bool is_TDMA_master = true;
unsigned char my_radio_client_ID = 0x7E;
unsigned int my_client_radio_connexion_state = 2;
int G_temperature_SI4463 = 32;
long int TDMA_table_TA[radio_addr_table_size] = {0, 100, 0, 250, 0, 0, 0};
unsigned short int G_radio_addr_table_RSSI[radio_addr_table_size] = {0, 0x5000, 0, 0x6400, 0, 0, 0};
unsigned short int G_radio_addr_table_BER[radio_addr_table_size] = {0, 250, 0, 1000, 0, 0, 0};
unsigned short int G_downlink_RSSI = 0;
unsigned short int G_downlink_BER = 0;
char CONF_radio_addr_table_status[radio_addr_table_size] = {0, 1, 0, 1, 0, 0, 0};
char CONF_radio_addr_table_callsign[radio_addr_table_size][16] = {{0}, {0, 0, 'D','L','6','A','B','E','-','7','1',0}, {0}, {0, 0, 'D','L','6','A','B','E','-','7','2',0}, {0}, {0}, {0}};
unsigned char G_reset_reason = 0;
const char FW_VERSION_ID[] = "261011G";
char CONF_radio_my_callsign[16] = {0, 0, 'D','L','6','A','B','E','-','7','0',0};

static uint8_t g_in[2048];
static uint32_t g_in_len = 0;
static uint8_t g_out[MAX_SNMPMSG_LEN * 2];
static int g_out_len = 0;
static W5500_chip g_chip;

uint16_t W5500_read_received_size(W5500_chip*, uint8_t) { return g_in_len; }
uint32_t W5500_read_UDP_pckt(W5500_chip*, uint8_t, unsigned char* data, uint32_t len) {
    uint32_t n = g_in_len < len ? g_in_len : len;
    memcpy(data, g_in, n);
    g_in_len = 0;
    return n;
}
void W5500_write_long(W5500_chip*, unsigned int, unsigned char, const uint8_t*, int) {}
void W5500_write_TX_buffer(W5500_chip*, uint8_t, unsigned char* data, int size, int) {
    g_out_len = size;
    memcpy(g_out, data, size);
}




int main(int argc, char **argv) {
    static char line[2048];
    if (argc > 1 && !strcmp(argv[1], "client")) { is_TDMA_master = false; my_radio_client_ID = 1; }
    if (argc > 2) G_reset_reason = (unsigned char)atoi(argv[2]);
    while (fgets(line, sizeof line, stdin)) {
        size_t n = strlen(line);
        while (n && (line[n-1] == '\n' || line[n-1] == '\r')) line[--n] = 0;
        // 8 byte W5500 UDP header (IP, port, length) followed by the SNMP message
        g_in_len = 0;
        for (int i = 0; i < 8; i++) g_in[g_in_len++] = 0;
        for (size_t i = 0; i + 1 < n; i += 2) {
            unsigned v; sscanf(line + i, "%2x", &v);
            g_in[g_in_len++] = (uint8_t)v;
        }
        g_out_len = 0;
        snmp_loop(&g_chip);
        if (g_out_len == 0) { printf("RSP:ERR no response\n"); fflush(stdout); continue; }
        printf("RSP:");
        for (int i = 0; i < g_out_len; i++) printf("%02x", g_out[i]);
        printf("\n");
        fflush(stdout);
    }
    return 0;
}
