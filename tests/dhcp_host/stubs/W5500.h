// Host stub: the chip is replaced by functions in driver.cpp that hand over and collect UDP packets.
#ifndef STUB_W5500_H
#define STUB_W5500_H
#include <stdint.h>
struct W5500_chip { int unused; };
#define DHCP_SOCKET 3
uint16_t W5500_read_received_size(W5500_chip*, uint8_t);
uint32_t W5500_read_UDP_pckt(W5500_chip*, uint8_t, unsigned char*, uint32_t);
void W5500_write_TX_buffer(W5500_chip*, uint8_t, unsigned char*, int, int);
#endif
