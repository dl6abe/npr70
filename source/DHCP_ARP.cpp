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

#include "DHCP_ARP.h"
#include "mbed.h"
#include "global_variables.h"
#include "Eth_IPv4.h"
#include "W5500.h"
#include "HMI_telnet.h"

// Content : DHCP server (for TDMA clients and point to point config)
// ARP proxy for "bridged ethernet" emulation. 
// ARP resolution (a little bit ugly)
 
#define DHCP_ARP_tab_size 32
#define DHCP_ARP_timeout 360 /*120*/
static unsigned char DHCP_ARP_MAC[DHCP_ARP_tab_size][6];
static unsigned long int DHCP_ARP_IP[DHCP_ARP_tab_size];
static unsigned char DHCP_ARP_status[DHCP_ARP_tab_size];	//		DHCP						ARP
															// 0:	Free						Free
															// 1:	allocation in progress		(unused)
															// 2:	allocated					Valid
															// 3:	prefered but not allocated	Timeout_1
static unsigned int DHCP_ARP_date[DHCP_ARP_tab_size];

int compare_IP(unsigned char* IP1, unsigned char* IP2) {
	int result;
	int i;
	result = 1;
	for (i=0; i<4; i++) {
		if (IP1[i] != IP2[i]) {
			result = 0;
		}
	}
	return result;
}

int compare_MAC(unsigned char* MAC1, unsigned char* MAC2) {
	int result;
	int i;
	result = 1;
	for (i=0; i<6; i++) {
		if (MAC1[i] != MAC2[i]) {
			result = 0;
		}
	}
	return result;
}

// *** DHCP functions ***

void reset_DHCP_table(LAN_conf_T* LAN_config) {
	int i;
	for (i=0; i<DHCP_ARP_tab_size; i++) {
		DHCP_ARP_status[i] = 0;
	}
	
}

void DHCP_release (LAN_conf_T* LAN_config, unsigned char* client_MAC) {
	int i;
	for (i=0; i<DHCP_ARP_tab_size; i++) {
		if ((compare_MAC( DHCP_ARP_MAC[i], client_MAC)) && (DHCP_ARP_status[i]))  {
			DHCP_ARP_status[i] = 0;
		}
	}
	
}

// Index of the DHCP table entry that holds this MAC (the last one if several), or 255.
static int DHCP_find_entry_by_MAC(unsigned char* client_MAC) {
	int i;
	int found = 255;
	for (i=0; i<DHCP_ARP_tab_size; i++) {
		if ((compare_MAC( DHCP_ARP_MAC[i], client_MAC)) && (DHCP_ARP_status[i]))  {
			found = i;
		}
	}
	return found;
}

// 1 if no other entry than 'own_entry' has this IP allocated.
static int DHCP_IP_is_free(unsigned long int IP, int own_entry) {
	int i;
	for (i=0; i<DHCP_ARP_tab_size; i++) {
		if ( (DHCP_ARP_IP[i] == IP) && (DHCP_ARP_status[i]==2) && (i != own_entry) ) {
			return 0;
		}
	}
	return 1;
}

// Lowest IP of the DHCP range that is not allocated. Returns 0 if the range is full.
static int DHCP_find_unused_IP(LAN_conf_T* LAN_config, unsigned long int* unused_IP) {
	int i, j;
	unsigned long int IP_tested;
	int OK_loc;
	int found_IP = 0;
	for (i = (LAN_config->DHCP_range_size - 1); i>=0; i--) {
		IP_tested = LAN_config->DHCP_range_start + i;
		OK_loc=1;
		for (j=0; j<DHCP_ARP_tab_size; j++) {
			if ( (IP_tested == DHCP_ARP_IP[j]) && (DHCP_ARP_status[j]==2) ) { // a améliorer : trop simple
				OK_loc = 0;
			}
		}
		if (OK_loc == 1) {
			*unused_IP = IP_tested;
			found_IP = 1;
		}
	}
	return found_IP;
}

// Store a new allocation in the lowest slot that is not allocated.
static void DHCP_store_entry(unsigned char* client_MAC, unsigned char* IP, unsigned char status) {
	int i;
	int free_DHCP_slot=255;
	for (i=DHCP_ARP_tab_size-1; i>=0; i--) {
		if (DHCP_ARP_status[i]!=2) {
			free_DHCP_slot = i;
		}
	}
	if (free_DHCP_slot!=255) {
		DHCP_ARP_status[free_DHCP_slot] = status;
		if (status == 2) {DHCP_ARP_date[free_DHCP_slot] = GLOBAL_timer.read_us();}
		DHCP_ARP_IP[free_DHCP_slot] = IP_char2int(IP);
		for (i=0; i<6; i++) {
			DHCP_ARP_MAC[free_DHCP_slot][i] = client_MAC[i];
		}
	}
}

int lookfor_free_LAN_IP (LAN_conf_T* LAN_config, unsigned char* client_MAC, unsigned char* requested_IP, unsigned char* proposed_IP, int req_type) {
	//req type : like DHCP. 1:discover 3:request
	int answer=0;
	int i;
	int new_alloc_entry = 0;
	unsigned char new_status = 0;
	unsigned long int req_IP_int = IP_char2int (requested_IP);
	unsigned long int unused_IP;

	if (req_type == 1) { new_status = 1; }
	if (req_type == 3) { new_status = 2; }

	// 1) tests
	// 1.1) lookfor client MAC in DHCP table
	int i_previous_alloc = DHCP_find_entry_by_MAC(client_MAC);
	// 1.2) check if matches previous allocation
	int match_previous_alloc = ( (i_previous_alloc!=255) && (DHCP_ARP_IP[i_previous_alloc] == req_IP_int) );
	if (match_previous_alloc) {
		// total match with previous allocation
		answer = 1;
		if (req_type==1) {
			DHCP_ARP_status[i_previous_alloc] = 1;
		}
		if (req_type==3) {
			DHCP_ARP_status[i_previous_alloc] = 2;
			DHCP_ARP_date[i_previous_alloc] = GLOBAL_timer.read_us();
		}
	}
	// 1.3) check if requested IP agreed : inside range, and 1.4) free (not allocated to another MAC)
	int requ_IP_inside_range = ( (req_IP_int >= LAN_config->DHCP_range_start) && (req_IP_int < (LAN_config->DHCP_range_start + LAN_config->DHCP_range_size)) );
	int req_IP_agreed = ( requ_IP_inside_range && DHCP_IP_is_free(req_IP_int, i_previous_alloc) );
	// 2.1) free previous DHCP entry
	if ( (i_previous_alloc!=255) && (match_previous_alloc==0) && (req_IP_agreed==1) ) {
		DHCP_ARP_status[i_previous_alloc] = 0;
	}
	// 2.2) look for unallocated IP
	if ( (req_type==1) && (req_IP_agreed==0) && (i_previous_alloc==255) ) {
		if (DHCP_find_unused_IP(LAN_config, &unused_IP)) {
			new_alloc_entry = 1;
			IP_int2char (unused_IP, proposed_IP);
			answer = 3;
		}
	}
	// 2.3) anwser with previous allocation 
	if ( (req_type == 1) && (req_IP_agreed==0) && (i_previous_alloc!=255) ) {
		IP_int2char (DHCP_ARP_IP[i_previous_alloc], proposed_IP);
		DHCP_ARP_status [i_previous_alloc] = 1;
		answer = 3;
	}
	// 2.4) new IP agreed, but do not match previous allocation
	if ( (i_previous_alloc!=255) && (match_previous_alloc==0) && (req_IP_agreed==1) ) {
		for (i=0; i<4; i++) {
			proposed_IP [i] = requested_IP[i]; 
		}
		new_alloc_entry = 1; 
		answer = 1;
	}
	// 2.5) new IP agreed and matches previous allocation
	if ( (i_previous_alloc!=255) && (match_previous_alloc==1) && (req_IP_agreed==1) ) {
		for (i=0; i<4; i++) {
			proposed_IP [i] = requested_IP[i]; 
		}
		if (req_type == 3) { 
			DHCP_ARP_date[i_previous_alloc] = GLOBAL_timer.read_us();
		}
		DHCP_ARP_status [i_previous_alloc] = new_status;
		answer = 1;
	}
	// 2.6) new IP agreed, no previous allocation
	if ( (i_previous_alloc==255) && (req_IP_agreed==1) ) {
		for (i=0; i<4; i++) {
			proposed_IP [i] = requested_IP[i]; 
		}
		new_alloc_entry = 1;
		answer = 1;
	}

	// new DHCP entry 
	if (new_alloc_entry == 1) {
		DHCP_store_entry(client_MAC, proposed_IP, new_status);
	}
	return answer;
}

// Reads MAC, session ID and requested IP from a DHCP request and returns the message type (option 53).
static unsigned char DHCP_parse_request(unsigned char* RX_data, int size_UDP, unsigned char* client_MAC, unsigned char* session_ID, unsigned char* requested_IP) {
	int i;
	int option_pos;
	unsigned char option_size;
	unsigned char option_type;
	unsigned char message_type_client = 0;
	for (i=0; i<6; i++) {
		client_MAC[i] = RX_data[i+36];
	}
	// session ID (XID)
	for (i=0; i<4; i++) {
		session_ID[i] = RX_data[i+12];
	}
	for (i=0; i<4; i++) {
		requested_IP[i] = RX_data[i+20];
	}
	// DHCP option read
	option_pos = 248;
	do {
		option_type = RX_data[option_pos];
		option_size = RX_data[option_pos+1];
		switch (option_type) {
			case 53 : message_type_client = RX_data[option_pos+2]; break;
			case 50 :
				requested_IP[0] = RX_data[option_pos+2];
				requested_IP[1] = RX_data[option_pos+3];
				requested_IP[2] = RX_data[option_pos+4];
				requested_IP[3] = RX_data[option_pos+5];
				break;
		} 
		option_pos = option_pos + option_size + 2;
	} while ((option_type != 255) && (option_pos < size_UDP));
	return message_type_client;
}

// Fields of the answer that do not depend on the message type. The options start at index 240.
static void DHCP_build_answer_header(unsigned char* DHCP_answer, unsigned char* session_ID, unsigned char* client_MAC) {
	int i;
	for (i=0; i<400; i++ )  {
		DHCP_answer[i] = 0;
	}
	DHCP_answer[0] = 0x02;//answer
	DHCP_answer[1] = 0x01;
	DHCP_answer[2] = 0x06;
	DHCP_answer[3] = 0x00;
	for (i=0; i<4; i++) {// session ID
		DHCP_answer[i+4] = session_ID[i];
	}
	for (i=0; i<6; i++) {// client MAC
		DHCP_answer[i+28] = client_MAC[i];
	}
	DHCP_answer[236] = 0x63;//magic cookie
	DHCP_answer[237] = 0x82;
	DHCP_answer[238] = 0x53;
	DHCP_answer[239] = 0x63;
}

static int DHCP_add_option_message_type(unsigned char* DHCP_answer, int pos, unsigned char message_type) {
	DHCP_answer[pos]=53; // message type
	DHCP_answer[pos+1]=1;
	DHCP_answer[pos+2]=message_type;
	return pos + 3;
}

static int DHCP_add_option_IP(unsigned char* DHCP_answer, int pos, unsigned char option, unsigned long int IP) {
	DHCP_answer[pos]=option;
	DHCP_answer[pos+1]=4;
	IP_int2char (IP, DHCP_answer + pos + 2);
	return pos + 6;
}

// Options of an OFFER (type 2) or an ACK (type 5). Returns the size of the answer.
static int DHCP_build_lease_answer(LAN_conf_T* LAN_config, unsigned char* DHCP_answer, unsigned char* proposed_IP, unsigned char message_type) {
	int i;
	int pos = 240;
	for (i=0; i<4; i++) { //proposed IP
		DHCP_answer[i+16] = proposed_IP[i];
	}
	pos = DHCP_add_option_message_type(DHCP_answer, pos, message_type);
	pos = DHCP_add_option_IP(DHCP_answer, pos, 1, LAN_config->LAN_subnet_mask);
	if (LAN_config->LAN_def_route_activ == 1) {
		pos = DHCP_add_option_IP(DHCP_answer, pos, 3, LAN_config->LAN_def_route);
	}
	if (LAN_config->LAN_DNS_activ == 1) {
		pos = DHCP_add_option_IP(DHCP_answer, pos, 6, LAN_config->LAN_DNS_value);
	}
	DHCP_answer[pos]=51; //Lease Time
	DHCP_answer[pos+1]=4;
	DHCP_answer[pos+2]=0x00;
	DHCP_answer[pos+3]=0x00;
	DHCP_answer[pos+4]= (DHCP_ARP_timeout & 0xFF00) >> 8;
	DHCP_answer[pos+5]= DHCP_ARP_timeout & 0xFF;
	pos = pos + 6;
	pos = DHCP_add_option_IP(DHCP_answer, pos, 54, LAN_config->LAN_modem_IP); // DHCP server IP
	DHCP_answer[pos]=255;// end
	return pos + 1;
}

/**
 * Runs the DHCP server routine that is regularly called from the main loop
 */
void DHCP_server(LAN_conf_T* LAN_config, W5500_chip* W5500 ) {
	static unsigned char RX_data[600];//600
	unsigned char client_MAC[7];
	unsigned char session_ID[5];
	unsigned char requested_IP[5]={0,0,0,0,0};
	int RX_size, size_UDP;
	unsigned char message_type_client;
	int answer_size;
	static unsigned char DHCP_answer[400];
	unsigned char proposed_IP[4] = {0,0,0,0};
	int loc_status;
	
	RX_size = W5500_read_received_size(W5500, DHCP_SOCKET); 
	if (RX_size>0) {
		size_UDP = W5500_read_UDP_pckt(W5500, DHCP_SOCKET, RX_data, sizeof(RX_data) - 8); // the parser reads up to 5 bytes behind the packet
		if (RX_data[8]==1) { // Valid DHCP request
			message_type_client = DHCP_parse_request(RX_data, size_UDP, client_MAC, session_ID, requested_IP);
			DHCP_build_answer_header(DHCP_answer, session_ID, client_MAC);
			
			// decision
			loc_status = lookfor_free_LAN_IP (LAN_config, client_MAC, requested_IP, proposed_IP, message_type_client);
			
			if (message_type_client == 1) { // discover -> offer
				answer_size = DHCP_build_lease_answer(LAN_config, DHCP_answer, proposed_IP, 2);
				W5500_write_TX_buffer(W5500, DHCP_SOCKET, DHCP_answer, answer_size, 1);
			}
			
			if ( (message_type_client == 3) && (loc_status !=0 ) )  { // request -> ack
				answer_size = DHCP_build_lease_answer(LAN_config, DHCP_answer, proposed_IP, 5);
				W5500_write_TX_buffer(W5500, DHCP_SOCKET, DHCP_answer, answer_size, 1);
			}
			
			if ( (message_type_client == 3) && (loc_status ==0 ) ) { //request -> NAK
				answer_size = DHCP_add_option_message_type(DHCP_answer, 240, 6);
				DHCP_answer[answer_size]=255;// end
				W5500_write_TX_buffer(W5500, DHCP_SOCKET, DHCP_answer, answer_size + 1, 1);
			}
			
			if ( message_type_client == 7 ) { // DHCP RELEASE
				DHCP_release (LAN_config, client_MAC); 
			}	
		}
	}
}

void DHCP_ARP_print_entries(void) {
	int i;
	unsigned char loc_IP_char[10];
	for (i=0; i<DHCP_ARP_tab_size; i++) {
		if (DHCP_ARP_status[i]!=0) {
			IP_int2char (DHCP_ARP_IP[i], loc_IP_char);
			HMI_printf ("   %i:stat:%i IP:", i, DHCP_ARP_status[i]);
			HMI_printf ("%i.%i.%i.%i MAC:", loc_IP_char[0], loc_IP_char[1], loc_IP_char[2], loc_IP_char[3]);
			HMI_printf("%02X:", DHCP_ARP_MAC[i][0]);
			HMI_printf("%02X:", DHCP_ARP_MAC[i][1]);
			HMI_printf("%02X:", DHCP_ARP_MAC[i][2]);
			HMI_printf("%02X:", DHCP_ARP_MAC[i][3]);
			HMI_printf("%02X:", DHCP_ARP_MAC[i][4]);
			HMI_printf("%02X ", DHCP_ARP_MAC[i][5]);
			HMI_printf("age:%isec\r\n", (GLOBAL_timer.read_us()-DHCP_ARP_date[i])/1000000);
		}
	}
	HMI_printf("\r\nready> ")
}

// *** ARP proxy and ARP resolver ***

void ARP_proxy (unsigned char* ARP_req_packet, int size) {
	int answer_needed = 0;
	int i;
	int is_inside_subnet = 0;
	int is_inside_client_range = 0; 
	unsigned long int ARP_client_IP;
	unsigned long int ARP_requested_IP;
	unsigned char ARP_client_MAC[6]; 
		
	unsigned char ARP_answ_packet[50] = {
		0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,	// dest MAC
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00,	// source MAC
		0x08, 0x06, 						// Ethtype ARP
		0x00, 0x01,							// HW type Ethernet
		0x08, 0x00, 						// protocol IPv4
		0x06, 								// HW addr size
		0x04, 								// protocol size
		0x00, 0x02	 						// ARP opcode "reply"
	};
	
	ARP_client_IP = IP_char2int (ARP_req_packet+28); //in request : client = sender
	ARP_requested_IP = IP_char2int (ARP_req_packet+38); //in request : server = target
	for (i=0; i<6; i++) {
		ARP_client_MAC[i] = ARP_req_packet[22+i]; // in request : client = sender
	}
	
	// determines if modem should reply or not 
	if (ARP_requested_IP != LAN_conf_applied.LAN_modem_IP) { //only replies to non-modem IP
	
		if (is_TDMA_master) { 
		//TDMA Master (and FDD down) answers to all IP in radio range
			if ( (ARP_requested_IP >= CONF_radio_IP_start) && (ARP_requested_IP < (CONF_radio_IP_start + CONF_radio_IP_size) ) ) {
				answer_needed = 1;
			}
		}
		else { //TDMA Slave answers to IP inside subnet, but which dont belong to it's own range
			if ( (ARP_requested_IP & LAN_conf_applied.LAN_subnet_mask) == (LAN_conf_applied.LAN_modem_IP & LAN_conf_applied.LAN_subnet_mask) ) {
				is_inside_subnet = 1;
			}
			if ( (ARP_requested_IP >= LAN_conf_applied.DHCP_range_start) && (ARP_requested_IP < (LAN_conf_applied.DHCP_range_start + LAN_conf_applied.DHCP_range_size)) ) {
				is_inside_client_range = 1;
			}
			if ( (is_inside_subnet == 1) && (is_inside_client_range == 0) ) {
				answer_needed = 1;
			}
		}
	}
	
	if (answer_needed) {
		for (i=0; i<6; i++) {				// Destination MAC
			ARP_answ_packet[i] = ARP_client_MAC[i];
		}
		for (i=0; i<6; i++) {				// Source MAC
			ARP_answ_packet[i+6] = CONF_modem_MAC[i];
		}
		for (i=0; i<6; i++) {				// ARP Sender MAC
			ARP_answ_packet[i+22] = CONF_modem_MAC[i];
		}
		//IP_int2char (LAN_config.LAN_modem_IP, (ARP_answ_packet + 28) ); // ARP sender IP
		IP_int2char (ARP_requested_IP, (ARP_answ_packet + 28) ); // ARP sender IP
		for (i=0; i<6; i++) {				// ARP Target MAC
			ARP_answ_packet[i+32] = ARP_client_MAC[i];
		}
		IP_int2char (ARP_client_IP, (ARP_answ_packet + 38) ); //ARP target IP
		
		W5500_write_TX_buffer(W5500_p1, RAW_SOCKET, ARP_answ_packet, 42, 0);
	}
}

void ARP_client_request (unsigned long int IP_requested) {
	unsigned char ARP_packet[50] = {
		0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,	// dest MAC
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00,	// source MAC
		0x08, 0x06, 						// Ethtype ARP
		0x00, 0x01,							// HW type Ethernet
		0x08, 0x00, 						// protocol IPv4
		0x06, 								// HW addr size
		0x04, 								// protocol size
		0x00, 0x01	 						// ARP opcode "request"
	};
	int i;
	for (i=0; i<6; i++) { // source MAC
		ARP_packet[i+6] = CONF_modem_MAC[i];
	}
	for (i=0; i<6; i++) { // ARP sender MAC
		ARP_packet[i+22] = CONF_modem_MAC[i];
	}
	IP_int2char (LAN_conf_applied.LAN_modem_IP, (ARP_packet + 28) ); //ARP sender IP
	for (i=0; i<6; i++) { // target MAC : unknown
		ARP_packet[i+32] = 0;
	}
	IP_int2char (IP_requested, (ARP_packet + 38) ); //ARP Target IP
	W5500_write_TX_buffer(W5500_p1, RAW_SOCKET, ARP_packet, 42, 0);
}

void ARP_client_answer_treatment (unsigned char* ARP_RX_packet, int size) {
	unsigned long int ARP_sender_IP;
	unsigned long int ARP_target_IP;
	unsigned char ARP_sender_MAC[6];
	int entry_already_exists = 0;
	int i_existing_entry = 0;
	int i_free_entry = 255;
	int i;
	
	ARP_sender_IP = IP_char2int (ARP_RX_packet+28);
	ARP_target_IP = IP_char2int (ARP_RX_packet+38);
	for (i=0; i<6; i++) {
		ARP_sender_MAC[i] = ARP_RX_packet[22+i];
	}
	if (ARP_target_IP == LAN_conf_applied.LAN_modem_IP) { //if ARP reply is for us
		//first look for existing entry
		//printf("ARP reply is for us IP %X\r\n", ARP_sender_IP);
		entry_already_exists = 0;
		for (i=0; i<DHCP_ARP_tab_size; i++) {
			if (DHCP_ARP_IP[i] == ARP_sender_IP) {
				entry_already_exists = 1;
				i_existing_entry = i;
				//printf ("matches %i\r\n", i);
			}
		}
		if (entry_already_exists == 1) { // replace existing entry
			//printf("ARP entry already exists %i\r\n", i_existing_entry);
			for (i=0; i<6; i++) {
				DHCP_ARP_MAC[i_existing_entry][i] = ARP_sender_MAC[i];
			}
			DHCP_ARP_status[i_existing_entry] = 2; // valid entry
			DHCP_ARP_date[i_existing_entry] = GLOBAL_timer.read_us();
		} 
		else { // no entry, lookfor empty slot
			for (i=DHCP_ARP_tab_size-1; i>=0; i--) {
				if ( DHCP_ARP_status[i] == 0 ) {
					i_free_entry = i;
				}
			}
			if (i_free_entry < DHCP_ARP_tab_size) {// free entry exists
				for (i=0; i<6; i++) {
					DHCP_ARP_MAC[i_free_entry][i] = ARP_sender_MAC[i];
				}
				DHCP_ARP_status[i_free_entry] = 2; // valid entry
				DHCP_ARP_IP[i_free_entry] = ARP_sender_IP;
				DHCP_ARP_date[i_free_entry] = GLOBAL_timer.read_us();
			}
		}
	}
	flush_temp_Eth_buffer (ARP_sender_IP);
} 

void ARP_RX_packet_treatment (unsigned char* ARP_RX_packet, int size) {
	unsigned int ARP_protocol_type;
	unsigned int ARP_opcode;
	unsigned long int ARP_sender_IP;
	unsigned long int ARP_target_IP;
	if (size < 42) { // Ethernet header (14) and ARP packet (28): the fields below are read up to byte 41
		return;
	}
	ARP_protocol_type = ARP_RX_packet[16]*0x100 + ARP_RX_packet[17];
	ARP_opcode = ARP_RX_packet[20]*0x100 + ARP_RX_packet[21];
	if ( (ARP_protocol_type == 0x0800) && (ARP_opcode == 0x0001) ) { // request
		//printf ("ARP request\r\n");
		ARP_sender_IP = IP_char2int (ARP_RX_packet+28);
		ARP_target_IP = IP_char2int (ARP_RX_packet+38);
		if (ARP_sender_IP != ARP_target_IP) {//ignores gratuitous ARP request
			//printf ("real ARP request\r\n");
			ARP_proxy(ARP_RX_packet, size);
		}
		else {
			//printf ("gratuitous!\r\n");
		}
	}
	if ( (ARP_protocol_type == 0x0800) && (ARP_opcode == 0x0002) ) { // reply
		//printf ("ARP reply\r\n");
		if ( (LAN_conf_applied.DHCP_server_active == 0) || (is_TDMA_master) ){//only active if modem is no DHCP server
			ARP_client_answer_treatment (ARP_RX_packet, size);
		}
	}
}

void DHCP_ARP_periodic_free_table () {
	int i;
	unsigned int loc_age;
	for (i=0; i<DHCP_ARP_tab_size; i++) {
		loc_age = GLOBAL_timer.read_us() - DHCP_ARP_date[i];
		if ( (DHCP_ARP_status[i] == 2) && (loc_age > 1000000 * DHCP_ARP_timeout) ) {
			DHCP_ARP_status[i] = 3;
		}
	}
}

// *** generic functions ***
int lookfor_MAC_from_IP (unsigned char* MAC_out, unsigned long int IP_addr) {
	int result = 0; 
	int i;
	int i_found = 300;
	unsigned int age_loc=0;
	if ( (LAN_conf_applied.DHCP_server_active == 1) && (!is_TDMA_master) ) { //resolution for DHCP
		i_found = 300;
		for (i=0; i<DHCP_ARP_tab_size; i++) {
			if ( (DHCP_ARP_IP[i] == IP_addr) && (DHCP_ARP_status[i] == 2) ) {
				i_found = i;
			}
		}
		if (i_found < DHCP_ARP_tab_size) {
			for (i=0; i<6; i++) {
				MAC_out[i] = DHCP_ARP_MAC[i_found][i];
			}
			result = 1;
		}
	}
	else { // resolution for ARP (DHCP not active)
		i_found = 300;
		for (i=0; i<DHCP_ARP_tab_size; i++) {
			if ( (DHCP_ARP_IP[i] == IP_addr) && ( (DHCP_ARP_status[i] == 2) || (DHCP_ARP_status[i] == 3) ) ) {
				i_found = i;
			}
		}
		if (i_found < DHCP_ARP_tab_size) { // existing entry
			age_loc = GLOBAL_timer.read_us() - DHCP_ARP_date[i_found];
			age_loc = age_loc / 1000000;
			if (age_loc < (DHCP_ARP_timeout / 2) ) {
				for (i=0; i<6; i++) {
					MAC_out[i] = DHCP_ARP_MAC[i_found][i];
				}
				result = 1;
			}
			else if (age_loc < DHCP_ARP_timeout) {
				for (i=0; i<6; i++) {
					MAC_out[i] = DHCP_ARP_MAC[i_found][i];
				}
				result = 1;
				ARP_client_request(IP_addr);
			}
			else {
				ARP_client_request(IP_addr); //
				IP_addr = 0; 
				result = 0;
			}
		}
		else {//no entry found, send ARP request
			ARP_client_request(IP_addr); //
			IP_addr = 0; 
			result = 0;
		}
	}
	return result;	// 0 if no entry found yet (most often: ARP resolution not yet answered)
					// 1 if entry present
}