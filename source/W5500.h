// This file is part of "NPR70 modem firmware" software
// (A GMSK data modem for ham radio 430-440MHz, at several hundreds of kbps) 
// Copyright (c) 2017-2020 Guillaume F. F4HDK (amateur radio callsign)
// 
// "NPR70 modem firmware" is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// "NPR70 modem firmware" is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with "NPR70 modem firmware".  If not, see <http://www.gnu.org/licenses/>

#ifndef W5500_F4
#define W5500_F4

#include "mbed.h"
#include "SPI_F4HDK.h"

// W5500 SPI commands work as follows:
// - Block address: base address for sockets, common settings, etc
// - Register address: inside the block, address of a specific register for that block
// Refer to the W5500 data sheet for details.

// W5500 Block addresses
#define W5500_Common_register_block    0x00
#define W5500_Socket_register_block(n) (1+4*n)
#define W5500_TX_Buffer_Block(n)       (2+4*n)
#define W5500_RX_Buffer_Block(n)       (3+4*n)

// W5500 Register addresses (using register names as defined in the W5500 datasheet)
#define W5500_Sn_MR     0x0000   // Socket n Mode
#define W5500_Sn_CR     0x0001   // Socket n Command
#define W5500_Sn_SR     0x0003   // Socket n Status
#define W5500_Sn_PORT0  0x0004   // Socket n Source Port
#define W5500_Sn_PORT1  0x0005
#define W5500_Sn_DHAR0  0x0006   // Socker n Destination Hardware address
#define W5500_Sn_DIPR0  0x000c   // Socket n Destination IP address
#define W5500_Sn_DPORT0 0x0010   // Socket n Destination Port
#define W5500_Sn_DPORT1 0x0011
#define W5500_Sn_RXBUF_SIZE 0x001e // Socket n Receive buffer size (in kB)
#define W5500_Sn_TXBUF_SIZE 0x001f // Socket n Transmit buffer size (in kB)
#define W5500_Sn_TX_FSR0 0x0020  // Socket n TX Free Size
#define W5500_Sn_RX_RSR0 0x0026  // Socket n RX Received Size
#define W5500_Sn_RX_RD0 0x0028   // Socket n RX Read pointer
#define W5500_Sn_RX_RD1 0x0029   // Socket n RX Read pointer
#define W5500_Sn_IMR    0x002c   // Socket n Interrupt Mask

// Socket Status register codes
#define W5500_SOCK_CLOSED      0x00
#define W5500_SOCK_INIT        0x13
#define W5500_SOCK_LISTEN      0x14
#define W5500_SOCK_ESTABLISHED 0x17
#define W5500_SOCK_WAIT        0x1c
#define W5500_SOCK_UDP         0x22
#define W5500_SOCK_MACRAW      0x42

#define W5500_SOCK_SYNSENT     0x15
#define W5500_SOCK_SYNRECV     0x16
#define W5500_SOCK_FIN_WAIT    0x18
#define W5500_SOCK_CLOSING     0x1a
#define W5500_SOCK_TIME_WAIT   0x1b
#define W5500_SOCK_LAST_ACK    0x1d

struct W5500_chip{
    SPI_F4HDK* spi_port;
    DigitalOut* cs;
	DigitalIn* interrupt;
	unsigned char sock_interrupt;
};

// NPR-specific defines
#define RAW_SOCKET    W5500_Socket_register_block(0)
#define TELNET_SOCKET W5500_Socket_register_block(1)
#define RTP_SOCKET    W5500_Socket_register_block(2)
#define DHCP_SOCKET   W5500_Socket_register_block(3)
#define FDD_SOCKET    W5500_Socket_register_block(4)
#define SNMP_SOCKET   W5500_Socket_register_block(5)

void W5500_read_long(W5500_chip* SPI_p_loc, unsigned int W5500_addr, unsigned char bloc_addr, unsigned char* RX_data, int RX_size);

void W5500_write_long(W5500_chip* SPI_p_loc, unsigned int W5500_addr, unsigned char bloc_addr, const uint8_t* TX_data, int TX_size);

void W5500_read_short(W5500_chip* SPI_p_loc, unsigned int W5500_addr, unsigned char bloc_addr, unsigned char* RX_data_ext);

void W5500_read_char(W5500_chip* SPI_p_loc, unsigned int W5500_addr, unsigned char bloc_addr, char* RX_data_ext, int RX_size);

uint8_t W5500_read_byte(W5500_chip* SPI_p_loc, unsigned int W5500_addr, unsigned char bloc_addr);

void W5500_write_byte(W5500_chip* SPI_p_loc, unsigned int W5500_addr, unsigned char bloc_addr, unsigned char data);


uint16_t W5500_read_received_size(W5500_chip* SPI_p_loc, uint8_t sock_nb);


void W5500_read_RX_buffer(W5500_chip* SPI_p_loc, uint8_t sock_nb,  uint8_t* data, int size);

uint32_t W5500_read_UDP_pckt (W5500_chip* SPI_p_loc, uint8_t sock_nb, unsigned char* data, uint32_t len);

int W5500_read_MAC_pckt (W5500_chip* SPI_p_loc, uint8_t sock_nb, unsigned char* data, int max_size);

void W5500_write_TX_buffer(W5500_chip* SPI_p_loc, uint8_t sock_nb, unsigned char* data, int size, int send_mac);

void W5500_re_configure (unsigned char why = 0); // why: bit mask of the changed LAN parameters (see signaling.cpp), 0 = other


void W5500_re_configure_periodic_call(W5500_chip* SPI_p_loc);

// Debug of the Ethernet PHY (command 'phy'): link tracking and a PHY reset without blocking
void W5500_link_poll(W5500_chip* SPI_p_loc);
unsigned int W5500_link_changes(void);        // number of link up/down changes since start
unsigned int W5500_link_last_change_s(void);  // seconds since start at the last change
void W5500_phy_reset_start(void);             // PHY reset, released about 300 ms later by W5500_link_poll
void W5500_phy_set_mode(W5500_chip* SPI_p_loc, unsigned char mode); // OPMDC (0 to 5, 7), not saved
int W5500_reconf_state(void);                 // 0 not configured, 1 ready, 2 to 4 re-configure running
unsigned int W5500_reconf_runs(void);         // number of re-configure requests since start
unsigned char W5500_reconf_why(void);         // reason mask of the last request
unsigned int W5500_reconf_last_s(void);       // seconds since start at the last request
unsigned int W5500_heal_count(void);          // PHY self-heal actions since start
unsigned int W5500_heal_last_s(void);         // seconds since start at the last action
unsigned int W5500_link_down_s(void);         // seconds the link has been down, 0 if up

void W5500_initial_configure(W5500_chip* SPI_p_loc); 

#endif