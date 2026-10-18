// Cases 8 and 10b. The real Eth_RX_dequeue, W5500_read_MAC_pckt, IPv4_to_radio, IPv4_from_radio run against
// a fake W5500 that holds one Ethernet frame.
//   8   frames of every size from 0 to 9000 bytes: no write behind the 1600 byte buffer on the stack, and
//       the handlers only get lengths that cover the fields they read
//  10b  IPv4 packet from the radio whose IP header claims more bytes than were received: nothing beyond the
//       received bytes may be sent on
#include "mbed.h"
#include "W5500.h"

#define radio_addr_table_size 7
struct LAN_conf_T {
	unsigned long int LAN_modem_IP, DHCP_range_start, DHCP_range_size, LAN_subnet_mask, LAN_def_route;
	unsigned char LAN_def_route_activ, LAN_DNS_activ;
	unsigned long int LAN_DNS_value;
	unsigned char DHCP_server_active;
};
Timer GLOBAL_timer;
LAN_conf_T LAN_conf_applied;
bool is_TDMA_master;
unsigned char CONF_master_FDD, CONF_modem_MAC[6] = {1, 2, 3, 4, 5, 6}, my_radio_client_ID;
unsigned long int CONF_radio_IP_start = 0xC0A80A03, CONF_radio_IP_size = 7;
unsigned long int CONF_radio_addr_table_IP_begin[radio_addr_table_size], CONF_radio_addr_table_IP_size[radio_addr_table_size];
unsigned int my_client_radio_connexion_state = 2;
int RX_Eth_IPv4_counter;
unsigned int G_eth_tx_ok, G_eth_tx_wait, G_radio_unrouted;
W5500_chip* W5500_p1;

static int failures;
static int fdd_calls, ipv4_calls, arp_calls, written_max;
void ARP_RX_packet_treatment(unsigned char*, int size) { arp_calls++; if (size < 14) { failures++; printf("MISMATCH ARP size %d\n", size); } }
void FDDdown_RX_pckt_treat(unsigned char*, int size) { fdd_calls++; if (size < 0) { failures++; printf("MISMATCH FDD size %d\n", size); } }
void segment_and_push(unsigned char* data, int size, unsigned char, unsigned char) { ipv4_calls++; (void)data; if (size < 20) { failures++; printf("MISMATCH IPv4 to radio size %d\n", size); } }
int lookfor_MAC_from_IP(unsigned char*, unsigned long int) { return 1; }
void W5500_write_TX_buffer(W5500_chip*, uint8_t, unsigned char*, int size, int) { if (size > written_max) written_max = size; }
void IPv4_to_radio(unsigned char*, int);
void IPv4_from_radio(unsigned char*, int);
unsigned char lookfor_client_ID_from_IP(unsigned long int);

static unsigned int pending;      // bytes of the frame in the chip, including the 2 length bytes
static unsigned int rd_pointer;
static unsigned int rd_written;
static unsigned int cmd_addr;
int SPI::write(const char* tx, int tx_len, char* rx, int rx_len) {
	if (tx && tx_len == 3 && rx) {
		unsigned int addr = ((unsigned char)tx[0] << 8) | (unsigned char)tx[1];
		memset(rx, 0, rx_len);
		if (addr == W5500_Sn_RX_RSR0 && rx_len >= 5) { rx[3] = pending >> 8; rx[4] = pending & 0xFF; }
		if (addr == W5500_Sn_RX_RD0 && rx_len >= 5) { rx[3] = rd_pointer >> 8; rx[4] = rd_pointer & 0xFF; }
		return 0;
	}
	if (tx && tx_len == 3 && !rx) { cmd_addr = ((unsigned char)tx[0] << 8) | (unsigned char)tx[1]; return 0; }
	if (tx && tx_len == 2 && !rx && cmd_addr == W5500_Sn_RX_RD0) { rd_written = ((unsigned char)tx[0] << 8) | (unsigned char)tx[1]; return 0; }
	if (!tx && rx && rx_len == 2) { rx[0] = pending >> 8; rx[1] = pending & 0xFF; return 0; }
	if (!tx && rx && rx_len > 0) { // frame bytes after the length: IPv4 over UDP to the modem, port 0x1A3C, or ARP
		memset(rx, 0x11, rx_len);
		extern int frame_kind;
		if (rx_len > 13) { rx[12] = 0x08; rx[13] = frame_kind ? 0x06 : 0x00; }
		if (rx_len > 37) { rx[23] = 0x11; rx[30] = 0xC0; rx[31] = 0xA8; rx[32] = 0x0A; rx[33] = 0x02; rx[36] = 0x1A; rx[37] = 0x3C; }
		return 0;
	}
	return 0;
}
int frame_kind;

#include "W5500_part.inc"
#include "W5500_mac.inc"
#include "eth_part.inc"

int main() {
	W5500_chip chip;
	SPI_F4HDK spi; DigitalOut cs; DigitalIn irq;
	chip.spi_port = &spi; chip.cs = &cs; chip.interrupt = &irq;
	LAN_conf_applied.LAN_modem_IP = 0xC0A80A02;
	LAN_conf_applied.LAN_subnet_mask = 0xFFFFFFF0;
	is_TDMA_master = 1;
	int sizes[] = {0, 1, 2, 3, 13, 15, 16, 17, 35, 36, 42, 43, 44, 45, 46, 60, 1000, 1514, 1515, 1516, 1517, 1598, 1599, 1600, 1601, 1602, 1700, 4000, 9000, 65535};
	for (int fdd = 0; fdd < 2; fdd++)
	for (int kind = 0; kind < 2; kind++)
	for (unsigned int k = 0; k < sizeof(sizes)/sizeof(sizes[0]); k++) {
		CONF_master_FDD = fdd;
		frame_kind = kind;
		pending = sizes[k];
		rd_pointer = 0x0200;
		rd_written = 0;
		Eth_RX_dequeue(&chip);
		if (sizes[k] >= 2 && rd_written != ((rd_pointer + sizes[k]) & 0xFFFF)) { failures++; printf("MISMATCH size %d: read pointer 0x%X, whole frame would be 0x%X\n", sizes[k], rd_written, (rd_pointer + sizes[k]) & 0xFFFF); }
	}
	printf("case 8: %d FDD, %d IPv4, %d ARP calls, %d failures\n", fdd_calls, ipv4_calls, arp_calls, failures);

	// 10b: IP header says 1500 bytes, 100 received
	static unsigned char frame[1600];
	memset(frame, 0, sizeof(frame));
	frame[16] = 0x05; frame[17] = 0xDC;                            // total length 1500
	frame[30] = 0xC0; frame[31] = 0xA8; frame[32] = 0x0A; frame[33] = 0x0B; // 192.168.10.11: inside the subnet, outside the radio range
	written_max = 0;
	IPv4_from_radio(frame, 100);
	printf("case 10b: claimed 1500 bytes, 100 received: %d bytes sent on\n", written_max);
	if (written_max > 100) failures++;
	frame[16] = 0x00; frame[17] = 60;                              // a good packet: 60 + 14 bytes
	written_max = 0;
	IPv4_from_radio(frame, 74);
	printf("case 10b: good packet of 74 bytes: %d bytes sent on\n", written_max);
	if (written_max != 74) failures++;
	written_max = 0;
	IPv4_from_radio(frame, 20);                                    // shorter than the address field
	printf("case 10b: 20 bytes received: %d bytes sent on\n", written_max);
	if (written_max != 0) failures++;
	return failures ? 1 : 0;
}
