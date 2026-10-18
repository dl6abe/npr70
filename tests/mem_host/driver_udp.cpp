// Case 1: an oversized UDP packet on the DHCP socket. The real W5500 read functions and the real
// DHCP_server run against a fake SPI that holds one UDP packet. Checks that nothing is written past
// RX_data[600] (ASan) and that the chip's read pointer moves past the whole packet.
#include "mbed.h"
#include "W5500.h"
#include "global_variables.h"
#include "Eth_IPv4.h"

Timer GLOBAL_timer;
#include "ip_conv.inc"

void W5500_write_TX_buffer(W5500_chip*, uint8_t, unsigned char*, int, int) {}

static unsigned int payload_size;     // UDP payload of the packet in the chip
static unsigned int rd_pointer;       // read pointer register, as the firmware sees it
static unsigned int rd_written = 0xFFFFFFFF;
static int pending;
static unsigned int cmd_addr;

int SPI::write(const char* tx, int tx_len, char* rx, int rx_len) {
	if (tx && tx_len == 3 && rx) { // register read: three command bytes, then the value
		unsigned int addr = ((unsigned char)tx[0] << 8) | (unsigned char)tx[1];
		memset(rx, 0, rx_len);
		if (addr == W5500_Sn_RX_RSR0 && rx_len >= 5) { rx[3] = (payload_size + 8) >> 8; rx[4] = (payload_size + 8) & 0xFF; }
		if (addr == W5500_Sn_RX_RD0 && rx_len >= 5) { rx[3] = rd_pointer >> 8; rx[4] = rd_pointer & 0xFF; }
		return 0;
	}
	if (tx && tx_len == 3 && !rx) { cmd_addr = ((unsigned char)tx[0] << 8) | (unsigned char)tx[1]; return 0; }
	if (tx && tx_len == 2 && !rx && cmd_addr == W5500_Sn_RX_RD0) { rd_written = ((unsigned char)tx[0] << 8) | (unsigned char)tx[1]; return 0; }
	if (!tx && rx && rx_len == 8) { // UDP header: IP, port, payload size
		memset(rx, 0, 8);
		rx[6] = payload_size >> 8; rx[7] = payload_size & 0xFF;
		return 0;
	}
	if (!tx && rx) { // payload: byte 0 = 1 so that it counts as a DHCP request
		memset(rx, 0x41, rx_len);
		if (rx_len > 0) rx[0] = 1;
		return 0;
	}
	return 0;
}

#include "DHCP_part.inc"

int main() {
	int ok = 1;
	W5500_chip chip;
	SPI_F4HDK spi; DigitalOut cs;
	chip.spi_port = &spi; chip.cs = &cs;
	LAN_conf_T lan;
	memset(&lan, 0, sizeof(lan));
	lan.DHCP_range_start = 0xC0A80A03; lan.DHCP_range_size = 7;
	unsigned int sizes[] = {300, 548, 584, 592, 700, 1000, 1400};
	for (unsigned int k = 0; k < sizeof(sizes)/sizeof(sizes[0]); k++) {
		payload_size = sizes[k];
		rd_pointer = 0x0100;
		rd_written = 0xFFFFFFFF;
		DHCP_server(&lan, &chip);
		unsigned int expect = rd_pointer + payload_size + 8;
		printf("payload %u: read pointer written 0x%X, whole packet would be 0x%X %s\n", payload_size, rd_written, expect, rd_written == expect ? "ok" : "MISMATCH");
		if (rd_written != expect) ok = 0;
	}
	return ok ? 0 : 1;
}
