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

#include "HMI_telnet.h"
#include "mbed.h"
#include "global_variables.h"
#include "SNMP.h"
#include "Eth_IPv4.h"
#include "signaling.h"
#include "config_flash.h"
#include "W5500.h"
#include "SI4463.h"
#include "TDMA.h"
#include "DHCP_ARP.h"
#include "L1L2_radio.h"

static char current_rx_line[100];
//static char HMI_out_str[100];
static int current_rx_line_count = 0;
static int rx_line_too_long = 0; // set when printable characters were dropped because the line buffer is full
static int is_telnet_opened = 0;
static int echo_ON = 1; 
static int display_status_ongoing = 0;
static int display_who_ongoing = 0;
static int slow_counter = 0;

static unsigned int telnet_last_activity;

/**
 * Called regularly by the main loop, and manages network events (new connection,
 * data, etc)
 */
int telnet_loop (W5500_chip* W5500) {
	static unsigned char previous_state = 0;
	uint8_t RX_data[100];
	uint8_t TX_data[300]; // echo of up to 97 received bytes, a backspace echoes 3 bytes
	static uint8_t iac_state = 0; // 0 normal, 1 after IAC, 2 after IAC WILL/WONT/DO/DONT (option byte follows)
	uint8_t current_state; // Socket state as returned by the W5500
	unsigned int timer_snapshot;
	char loc_char;
	int RX_size = 0;
	int i, j;
	int result;

	result=0;
	
	current_state = W5500_read_byte(W5500, W5500_Sn_SR, TELNET_SOCKET);
	//printf("state: %x\r\n", current_state);
	if ((current_state == W5500_SOCK_ESTABLISHED) && (previous_state != W5500_SOCK_ESTABLISHED)) {
		W5500_read_long(W5500, W5500_Sn_DIPR0, TELNET_SOCKET, RX_data, 7);
		// Note: RX_data starts with 3 extra bytes (W5500 register that was read) that we don't want
		printf("\r\n\r\nnew telnet connexion from %i.%i.%i.%i\r\nserial inactive...\r\n", RX_data[3], RX_data[4], RX_data[5], RX_data[6]);
		fflush(stdout);
		//TX_data[0] = 0xFF; //IAC
		//TX_data[1] = 0xFB; //WILL FB DO FD
		//TX_data[2] = 1; //echo 1 RTCE 7
		//TX_data[3] = 0;
		TX_data[0] = 0xFF; //IAC
		TX_data[1] = 0xFB; //WILL 
		TX_data[2] = 0x01; //Echo
		//TX_data[3] = 0;
		TX_data[3] = 0xFF; //IAC
		TX_data[4] = 0xFD; //DO
		TX_data[5] = 0x03; //Suppr GA 
		TX_data[6] = 0xFF; //IAC
		TX_data[7] = 0xFB; //WILL
		TX_data[8] = 0x03; //Suppr GA 
		TX_data[9] = 0;
		strcat((char*)TX_data, "NPR modem\r\nready> ");
		W5500_write_TX_buffer (W5500, TELNET_SOCKET, TX_data, 27, 0); //27
		//HMI_printf("ready>");
		is_telnet_opened = 1;
		iac_state = 0;
		rx_line_too_long = 0;
		current_rx_line_count = 0;
		echo_ON = 1;
		display_status_ongoing = 0;
		display_who_ongoing = 0;
		telnet_last_activity = GLOBAL_timer.read_us();
	}
	
	if (current_state == W5500_SOCK_WAIT) { // close wait to close 
		W5500_write_byte(W5500, W5500_Sn_CR, TELNET_SOCKET, 0x10); 
		printf("telnet connexion closed\r\nready> "); 
		fflush(stdout);
		is_telnet_opened = 0;
		current_rx_line_count = 0;
		echo_ON = 1;
		display_status_ongoing = 0;
		display_who_ongoing = 0;
	}
	
	if (current_state == W5500_SOCK_CLOSED) { //closed to open
		W5500_write_byte(W5500, W5500_Sn_CR, TELNET_SOCKET, 0x01); 
		//printf("open \r\n");
		result=1;
	}
	
	if (current_state == W5500_SOCK_INIT) { //opened to listen
		W5500_write_byte(W5500, W5500_Sn_CR, TELNET_SOCKET, 0x02); 
		//printf("listen \r\n");
	}
	previous_state = current_state;
	if (is_telnet_opened) {
		RX_size = W5500_read_received_size(W5500, TELNET_SOCKET); 
		timer_snapshot = GLOBAL_timer.read_us();
		if ((timer_snapshot - telnet_last_activity) > 300000000) { //300000000
			//HMI_printf("Telnet inactivity timeout. Force exit.\r\n");
			W5500_write_byte(W5500_p1, W5500_Sn_CR, TELNET_SOCKET, 0x08); //close TCP
			is_telnet_opened = 0;
			echo_ON = 1;
			display_status_ongoing = 0;
			display_who_ongoing = 0;
			printf("telnet connexion closed\r\nready> "); 
			fflush(stdout);
		}
		//timeout 
	}
	if (RX_size > 97) {
		RX_size = 97; // RX_data holds 3 header bytes plus the data, the rest is read in the next call
	}
	if (RX_size > 0) {
		telnet_last_activity = GLOBAL_timer.read_us();
		result=1;
		// printf("RX Size: %i\r\n", RX_size);
		W5500_read_RX_buffer(W5500, TELNET_SOCKET, RX_data, RX_size+3);
		// Note: RX_data starts with three extra bytes (W5500 register/block info) that are not
		// part of the actual received data
		i = 3;
		j = 0;
		while (i < (RX_size+3)) {
			loc_char = (char)RX_data[i];
			if (iac_state == 1) { // second byte of a telnet command, may be in a later read than the IAC
				if (RX_data[i] == 244) {//ctrl+C
					HMI_cancel_current();
				}
				iac_state = ((RX_data[i] >= 251) && (RX_data[i] <= 254)) ? 2 : 0; // WILL WONT DO DONT take an option byte
				i++;
				continue;
			}
			if (iac_state == 2) { // option byte, ignored
				iac_state = 0;
				i++;
				continue;
			}
			// printf("%02X %c\r\n", loc_char, loc_char);
			if ( (loc_char >= 0x20) && (loc_char <= 0x7E) ) {//displayable char
				if ( (current_rx_line_count < 98) && (echo_ON) ) {
					TX_data[j]=RX_data[i];
					i++;
					j++;
					current_rx_line[current_rx_line_count] = loc_char;
					current_rx_line_count++;
				} else {
					if (echo_ON) {
						rx_line_too_long = 1;
					}
					i++;
				}
			} 
			else { // special char
				if ((uint8_t)loc_char == 0xFF) {//IAC, handled byte by byte with iac_state
					iac_state = 1;
					i++;
				}
				else if ( ( (loc_char == 0x08) || (loc_char == 0x7F) ) && (echo_ON) ) { //backspace
					i++;
					
					if (current_rx_line_count>0) {
						current_rx_line_count--;
						TX_data[j] = 0x08;
						TX_data[j+1] = 0x20;
						TX_data[j+2] = 0x08;
						j=j+3;
					}
				}
				else if ( (loc_char == 0x0D) && (echo_ON) ){ //end of line
					TX_data[j] = 0x0D;
					TX_data[j+1] = 0x0A;
					i++;
					j = j + 2;
					current_rx_line[current_rx_line_count] = 0;//null termination
					current_rx_line_count++;
					W5500_write_TX_buffer (W5500, TELNET_SOCKET, TX_data, j, 0);
					j = 0;
					if (rx_line_too_long) { // do not run a cut-off command
						rx_line_too_long = 0;
						HMI_printf("ERROR: line too long, maximum is 98 characters.\r\nready> ");
					} else {
						HMI_line_parse (current_rx_line, current_rx_line_count);
					}
					current_rx_line_count = 0;
				}
				else if (loc_char == 0x03) { //ctrl + C
					HMI_cancel_current();
					//printf("CTRL + C\r\n");
					i++;
				} else {
					i++;
				}
			}
		}
		if (j > 0) {
			W5500_write_TX_buffer (W5500, TELNET_SOCKET, TX_data, j, 0);
		}
		//printf("\r\n");
	}
	return result;
}

int serial_term_loop (void) {
	char loc_char;
	
	if (pc.readable()) {
		loc_char = getc(pc);
		
		if (is_telnet_opened == 0) {
			if ( (loc_char >= 0x20) && (loc_char <= 0x7E) ) {//printable char
				if ( (current_rx_line_count < 98) && (echo_ON) ) {
					putc(loc_char, pc);
					current_rx_line[current_rx_line_count] = loc_char;
					current_rx_line_count++;
				} else if (echo_ON) {
					rx_line_too_long = 1;
				}
			}
			else {
				if ( ( (loc_char == 0x08) || (loc_char == 0x7F) ) && (echo_ON) ) {//backspace
					if (current_rx_line_count>0) {
						current_rx_line_count--;
						putc(0x08,pc);
						putc(0x20,pc);
						putc(0x08,pc);
					}
				}
				else if ( (loc_char == 0x0D) && (echo_ON) ) {
					printf("\r\n");
					current_rx_line[current_rx_line_count] = 0;
					current_rx_line_count++;
					if (rx_line_too_long) {
						rx_line_too_long = 0;
						HMI_printf("ERROR: line too long, maximum is 98 characters.\r\nready> ");
					} else {
						HMI_line_parse (current_rx_line, current_rx_line_count);
					}
					current_rx_line_count = 0;
				}
				else if (loc_char == 0x03) {//ctrl + c
					HMI_cancel_current();
				}
			}
		}
		return 1;
	} else {
		return 0;
	}
}

static void HMI_print_help(void);
static void HMI_debug_command(void);

// One line per call, the console output buffer is small.
static void HMI_print_help(void) {
	HMI_printf("Commands:\r\n");
	HMI_printf("  display config      show the configuration (show works as well)\r\n");
	HMI_printf("  display static      show the static address allocation\r\n");
	HMI_printf("  display DHCP_ARP    show the DHCP and ARP entries\r\n");
	HMI_printf("  set <param> <value> change a parameter, then 'save'\r\n");
	HMI_printf("  save                write the configuration to flash\r\n");
	HMI_printf("  reset_to_default    erase the saved configuration\r\n");
	HMI_printf("  radio on|off        switch the radio\r\n");
	HMI_printf("  status              live link status, Ctrl+C to leave\r\n");
	HMI_printf("  who                 live master and client list, Ctrl+C to leave\r\n");
	HMI_printf("  clear stats         set the received frame counters back to 0\r\n");
	HMI_printf("  TX_test <seconds>   transmit test, the radio must be off\r\n");
	HMI_printf("  version             firmware version and band\r\n");
	HMI_printf("  uptime              time since the start of the modem\r\n");
	HMI_printf("  phy                 Ethernet PHY status (debug), 'phy reset', 'phy mode <0-5|7>'\r\n");
	HMI_printf("  debug               everything for a fault report in one output (no live display)\r\n");
	HMI_printf("  reboot              restart the modem\r\n");
	HMI_printf("  exit                close the telnet session (logout works as well)\r\n");
	HMI_printf("Radio parameters:\r\n");
	HMI_printf("  frequency freq_shift RF_power modulation radio_netw_ID radio_on_at_start\r\n");
	HMI_printf("Identity and mode:\r\n");
	HMI_printf("  callsign is_master master_FDD trans_method\r\n");
	HMI_printf("Network parameters:\r\n");
	HMI_printf("  modem_IP netmask IP_begin master_IP_size client_req_size client_static_IP\r\n");
	HMI_printf("  DHCP_active def_route_active def_route_val DNS_active DNS_value master_down_IP\r\n");
	HMI_printf("  telnet_active telnet_routed Eth_mode (0 to 7, 7 = auto)\r\n");
	HMI_printf("ready> ");
}

// Debug of the Ethernet PHY and of the RJ45 LEDs: status, reset, temporary mode (not saved)
static void HMI_phy_lines(void) {
	unsigned char reg;
	unsigned int up;
	reg = W5500_read_byte(W5500_p1, 0x002E, W5500_Common_register_block);
	HMI_printf("PHYCFGR 0x%02X: link %s, %s, %s duplex, OPMD %i, OPMDC %i, config Eth_mode %i\r\n", reg, (reg & 0x01) ? "UP" : "DOWN", (reg & 0x02) ? "100M" : "10M", (reg & 0x04) ? "full" : "half", (reg >> 6) & 1, (reg >> 3) & 7, CONF_Eth_mode);
	HMI_printf("W5500 VERSIONR 0x%02X (expect 0x04)\r\n", W5500_read_byte(W5500_p1, 0x0039, W5500_Common_register_block));
	up = (unsigned int)snmp_uptime_seconds();
	HMI_printf("link changes since start: %u, last change at uptime %u s (now %u s)\r\n", W5500_link_changes(), W5500_link_last_change_s(), up);
	HMI_printf("re-configure state %i (1 = ready), runs %u, last reason 0x%02X at uptime %u s\r\n", W5500_reconf_state(), W5500_reconf_runs(), (unsigned int)W5500_reconf_why(), W5500_reconf_last_s());
	HMI_printf("PHY self-heal: %u actions, last at uptime %u s, link down for %u s\r\n", W5500_heal_count(), W5500_heal_last_s(), W5500_link_down_s());
	HMI_printf("GPIOA input: PA2=%i PA13=%i PA14=%i PA15=%i (ST-LINK lines, PA2 VCP TX, PA15 VCP RX)\r\n", (int)((GPIOA->IDR >> 2) & 1), (int)((GPIOA->IDR >> 13) & 1), (int)((GPIOA->IDR >> 14) & 1), (int)((GPIOA->IDR >> 15) & 1));
}

static void HMI_phy_command(char* arg1, char* arg2) {
	unsigned int mode;
	if (arg1 && (strcmp(arg1, "reset") == 0)) {
		W5500_phy_reset_start();
		HMI_printf("PHY reset, released after 300 ms, the Ethernet link restarts\r\nready> ");
		return;
	}
	if (arg1 && (strcmp(arg1, "mode") == 0)) {
		if ( (arg2 == NULL) || (sscanf(arg2, "%u", &mode) != 1) || (mode > 7) || (mode == 6) ) {
			HMI_printf("ERROR: use 'phy mode <0-5|7>' (6 = power down is not allowed)\r\nready> ");
			return;
		}
		W5500_phy_set_mode(W5500_p1, (unsigned char)mode);
		HMI_printf("PHY mode %u set (not saved, 7 = auto negotiation)\r\nready> ", mode);
		return;
	}
	if (arg1) {
		HMI_printf("ERROR: use 'phy', 'phy reset' or 'phy mode <0-5|7>'\r\nready> ");
		return;
	}
	HMI_phy_lines();
	HMI_printf("ready> ");
}

void HMI_line_parse (char* RX_text, int RX_text_count) {
	char* loc_command_str;// [100];
	char* loc_param1_str;
	char* loc_param2_str;
	int command_understood = 0;
	int temp;

	loc_command_str = strtok (RX_text, " ");
	loc_param1_str = strtok (NULL, " ");
	loc_param2_str = strtok (NULL, " ");

	if (loc_command_str) {
		if (strcmp(loc_command_str, "radio") == 0) {
			command_understood = 1;
			if (loc_param1_str == NULL) {
				HMI_printf("ERROR: use 'radio on' or 'radio off'.\r\nready> ");
			}
			else if (strcmp(loc_param1_str, "on") == 0) {
				if (CONF_radio_state_ON_OFF == 0) {
					RADIO_on(1, 1, 1);
				}
				HMI_printf("OK\r\nready> ");
			}
			else if (strcmp(loc_param1_str, "off") == 0) {
				RADIO_off(1);
				HMI_printf("OK\r\nready> ");
			}
			else {
				HMI_printf("ERROR: unknown parameter for 'radio', use 'radio on' or 'radio off'.\r\nready> ");
			}
		}
		
		if (strcmp(loc_command_str, "TX_test") == 0) {
			command_understood = 1;
			if (loc_param1_str) {
				HMI_TX_test(loc_param1_str);
			} else {
				HMI_printf("ERROR: use 'TX_test <seconds>'.\r\nready> ");
			}
		}
		if (strcmp(loc_command_str, "status") == 0) {//display status
			command_understood = 1;
			display_status_ongoing = 1;
			G_downlink_bandwidth_temp = 0;
			G_uplink_bandwidth_temp = 0;
			slow_counter = 0;
			echo_ON = 0;
			HMI_periodic_call();
		}
		// display, show is the same command
		if ((strcmp(loc_command_str, "display") == 0) || (strcmp(loc_command_str, "show") == 0)) {
			command_understood = 1;
			if (loc_param1_str == NULL) {
				HMI_printf("ERROR: use 'display config', 'display static' or 'display DHCP_ARP'.\r\nready> ");
			}
			else if (strcmp(loc_param1_str, "config") == 0) {//display config
				HMI_display_config();
			}
			else if (strcmp(loc_param1_str, "static") == 0) {//display static alloc
				HMI_display_static();
			}
			else if (strcmp(loc_param1_str, "DHCP_ARP") == 0) {//display DHCP_ARP entries
				DHCP_ARP_print_entries();
			}
			else {
				HMI_printf("ERROR: unknown argument for display/show, type 'help' for the list.\r\nready> ");
			}
		}
		if (strcmp(loc_command_str, "set") == 0) {
			HMI_set_command(loc_param1_str, loc_param2_str);
			command_understood = 1;
		}
		if (strcmp(loc_command_str, "who") == 0) {
			//HMI_print_who();
			command_understood = 1;
			display_who_ongoing = 1;
			slow_counter = 0;
			echo_ON = 0;
			HMI_periodic_call();
		}
		if (strcmp(loc_command_str, "reboot") == 0) {
			command_understood = 1;
			HMI_reboot();
		}
		if (strcmp(loc_command_str, "save") == 0) {
			command_understood = 1;

			RADIO_off_if_necessary(0);
			temp = NFPR_config_save();
			RADIO_restart_if_necessary(0, 0, 1);
			HMI_printf("Configuration saved. Index: %i.\r\nready> ", temp);
		}
		if (strcmp(loc_command_str, "reset_to_default") == 0) {
			command_understood = 1;
			HMI_printf("clearing saved config...\r\n");
			RADIO_off_if_necessary(0);
			virt_EEPROM_errase_all();
			HMI_printf("Done. Now rebooting...\r\n");
			NVIC_SystemReset();
		}
		if (strcmp(loc_command_str, "clear") == 0) {
			command_understood = 1;
			if ( (loc_param1_str != NULL) && (strcmp(loc_param1_str, "stats") == 0) ) {
				radio_clear_fec_stats();
				HMI_printf("received frame counters cleared\r\nready> ");
			} else {
				HMI_printf("ERROR: use 'clear stats'.\r\nready> ");
			}
		}
		if (strcmp(loc_command_str, "uptime") == 0) {
			command_understood = 1;
			uint32_t up = snmp_uptime_seconds(); // the clock SNMP sysUpTime uses
			HMI_printf("uptime: %lu days %02lu:%02lu:%02lu\r\nready> ", (unsigned long)(up / 86400), (unsigned long)((up / 3600) % 24), (unsigned long)((up / 60) % 60), (unsigned long)(up % 60));
		}
		if (strcmp(loc_command_str, "phy") == 0) {
			command_understood = 1;
			HMI_phy_command(loc_param1_str, loc_param2_str);
		}
		if (strcmp(loc_command_str, "debug") == 0) {
			command_understood = 1;
			HMI_debug_command();
		}
		if (strcmp(loc_command_str, "version") == 0) {
			command_understood = 1;
			HMI_printf(FW_BANNER_FORMAT "freq band: %s\r\nready> ", FW_VERSION, MBED_OS_VERSION, snmp_reset_reason_text(G_reset_reason), FREQ_BAND);
		}
		// exit, logout is the same command
		if ((strcmp(loc_command_str, "exit") == 0) || (strcmp(loc_command_str, "logout") == 0)) {
			command_understood = 1;
			HMI_exit();
		}
		if ((strcmp(loc_command_str, "help") == 0) || (strcmp(loc_command_str, "?") == 0)) {
			command_understood = 1;
			HMI_print_help();
		}
		if (command_understood == 0) {
			HMI_printf("Unknown command. Type 'help' for the list.\r\nready> ");
		}
	} else {//just a return with nothing
		HMI_printf("ready> ");
	}
}

void HMI_cancel_current(void) {
	if (echo_ON ==0) {
		echo_ON = 1;
		display_status_ongoing = 0;
		display_who_ongoing = 0;
		HMI_printf("ready> ");
	}
}

int HMI_check_radio_OFF(void) {
	if (CONF_radio_state_ON_OFF == 1) {
		HMI_printf("radio must be off for this command\r\nready> ");
		return 0;
	} else {
		return 1;
	}
}


void HMI_TX_test(char* duration_txt) {
	unsigned int duration;
	if ( HMI_check_radio_OFF() == 1) {
		sscanf (duration_txt, "%i", &duration);
		HMI_printf("reconfiguring radio...\r\n");
		SI4463_configure_all();
		ThisThread::sleep_for(50);
		TDMA_init_all();
		//SI4463_radio_start();
		
		ThisThread::sleep_for(1);
		G_SI4463->RX_TX_state = 0;
		SI4463_clear_IT(G_SI4463, 0, 0);
		ThisThread::sleep_for(10);
		CONF_radio_state_ON_OFF = 1;
		SI4463_TX_to_RX_transition();
		
		ThisThread::sleep_for(10);
		CONF_radio_state_ON_OFF = 0;
		if (!is_TDMA_master) {
			my_client_radio_connexion_state = 1;
			my_radio_client_ID = 0x7E;
		}
		ThisThread::sleep_for(50);
		
		TDMA_NULL_frame_init(70);
		HMI_printf("radio transmit test %i seconds...\r\n", duration);
		duration = duration * 1000; //milliseconds instead of seconds
		
		SI4432_TX_test(duration);
		
		HMI_printf("ready> ");
	}
}

void HMI_reboot(void) {
	if (is_telnet_opened == 1) {
		W5500_write_byte(W5500_p1, 0x0001, TELNET_SOCKET, 0x08);
	}
	//extern "C" void mbed_reset();
	NVIC_SystemReset();
}

void HMI_force_exit(void) {
	unsigned char IP_loc[8];
	if (is_telnet_opened == 1) {
		IP_int2char (LAN_conf_applied.LAN_modem_IP, IP_loc);
		//HMI_printf("\r\n\r\n\r\nNew IP config... force telnet exit.\r\n");
		//HMI_printf("\r\n\r\nNew IP config. Open new telnet session with: %i.%i.%i.%i\r\n\r\n", IP_loc[0], IP_loc[1], IP_loc[2], IP_loc[3]);
		W5500_write_byte(W5500_p1, 0x0001, TELNET_SOCKET, 0x08); //close TCP
		is_telnet_opened = 0;
		echo_ON = 1;
		display_status_ongoing = 0;
		printf("telnet connexion closed\r\nready> "); 
		fflush(stdout);
	}
}

void HMI_exit(void) {
	if (is_telnet_opened == 1) {
		W5500_write_byte(W5500_p1, 0x0001, TELNET_SOCKET, 0x08); //close TCP
		is_telnet_opened = 0;
		echo_ON = 1;
		display_status_ongoing = 0;
		printf("telnet connexion closed\r\nready> "); 
		fflush(stdout);
	} else {
		printf("exit only valid for telnet\r\nready> ");
		fflush(stdout);
	}
}

static char HMI_yes_no[2][4]={'n','o',0,0, 'y','e','s',0};
// static char HMI_trans_modes[2][4]={'I','P',0,0,'E','t','h',0};
static char HMI_master_FDD[3][5]={'n','o',0,0,0,'d','o','w','n',0,'u','p',0,0,0};

static void HMI_config_lines(void) {
	unsigned char IP_loc[8];

	HMI_printf("CONFIG:\r\n  callsign: '%s'\r\n  is_master: %s\r\n  MAC: %02X:%02X:%02X:%02X:%02X:%02X\r\n", CONF_radio_my_callsign+2, HMI_yes_no[is_TDMA_master],CONF_modem_MAC[0],CONF_modem_MAC[1],CONF_modem_MAC[2],CONF_modem_MAC[3],CONF_modem_MAC[4],CONF_modem_MAC[5]);
	HMI_printf("  Eth_mode: %i\r\n", CONF_Eth_mode);
	HMI_printf("  ext_SRAM: %s\r\n", HMI_yes_no[is_SRAM_ext]);
	HMI_printf("  frequency: %.3fMHz\r\n  freq_shift: %.3fMHz\r\n  RF_power: %i\r\n  modulation: %i\r\n", ((float)CONF_frequency_HD/1000)+FREQ_RANGE_MIN, (float)CONF_freq_shift/1000, CONF_radio_PA_PWR, CONF_radio_modulation); 

	HMI_printf("  radio_netw_ID: %i\r\n  radio_on_at_start: %s\r\n", CONF_radio_network_ID, HMI_yes_no[CONF_radio_default_state_ON_OFF]);
	HMI_printf("  telnet active: %s\r\n  telnet routed: %s\r\n", HMI_yes_no[is_telnet_active], HMI_yes_no[is_telnet_routed]);
	IP_int2char (LAN_conf_saved.LAN_modem_IP, IP_loc);
	IP_int2char (LAN_conf_saved.LAN_subnet_mask, IP_loc+4);
	HMI_printf("  modem_IP: %i.%i.%i.%i\r\n  netmask: %i.%i.%i.%i\r\n", IP_loc[0], IP_loc[1],IP_loc[2],IP_loc[3],IP_loc[4],IP_loc[5],IP_loc[6],IP_loc[7]);
	
	if (is_TDMA_master) {
		HMI_printf("  master_FDD: %s\r\n", HMI_master_FDD[CONF_master_FDD]);
	}
	
	
	if ( (is_TDMA_master) && ( CONF_master_FDD < 2 ) && (CONF_transmission_method==0) ) {//Master FDD down (or no FDD)	
		IP_int2char (CONF_radio_IP_start, IP_loc);
		IP_int2char (CONF_radio_IP_start+CONF_radio_IP_size-1, IP_loc+4);
		HMI_printf("  IP_begin: %i.%i.%i.%i\r\n  master_IP_size: %ld (Last IP: %i.%i.%i.%i)\r\n", IP_loc[0], IP_loc[1],IP_loc[2],IP_loc[3],CONF_radio_IP_size, IP_loc[4],IP_loc[5],IP_loc[6],IP_loc[7]);
		IP_int2char (LAN_conf_saved.LAN_def_route, IP_loc);	
		HMI_printf("  def_route_active: %s\r\n  def_route_val: %i.%i.%i.%i\r\n", HMI_yes_no[LAN_conf_saved.LAN_def_route_activ], IP_loc[0],IP_loc[1],IP_loc[2],IP_loc[3]);
		IP_int2char (LAN_conf_saved.LAN_DNS_value, IP_loc);
		HMI_printf("  DNS_active: %s\r\n  DNS_value: %i.%i.%i.%i\r\n", HMI_yes_no[LAN_conf_saved.LAN_DNS_activ], IP_loc[0],IP_loc[1],IP_loc[2],IP_loc[3]);
	}
	if ( (is_TDMA_master) && (CONF_master_FDD == 2) ) {//Master FDD up
		IP_int2char (CONF_master_down_IP, IP_loc);
		HMI_printf("  master_down_IP: %i.%i.%i.%i\r\n",IP_loc[0],IP_loc[1],IP_loc[2],IP_loc[3]);
	}
	if (!is_TDMA_master) {//client
		IP_int2char (CONF_radio_IP_start, IP_loc);
		HMI_printf("  IP_begin: %i.%i.%i.%i\r\n", IP_loc[0], IP_loc[1],IP_loc[2],IP_loc[3]);
		HMI_printf("  client_req_size: %ld\r\n  DHCP_active: %s\r\n", CONF_radio_IP_size_requested, HMI_yes_no[LAN_conf_saved.DHCP_server_active]);
	}
}

void HMI_display_config(void) {
	HMI_config_lines();
	HMI_printf("ready> ");
}

// One output with what a fault report needs: state of the link, the radio, the Ethernet side, the
// counters and the configuration. Everything is read, nothing is changed. Lines stay short, the
// console buffer is HMI_out_str[120].
static void HMI_debug_command(void) {
	static const char* const state_text[6] = {"unknown", "waiting connection", "connected", "rejected", "disconnecting", "disconnected"};
	unsigned char IP_c[8];
	unsigned int state, alloc_age, now_us, i;
	unsigned char mf_ID, mf_mask;
	int since_request, since_ack;
	unsigned long int up;
	now_us = GLOBAL_timer.read_us();
	up = snmp_uptime_seconds();
	HMI_printf("== debug ==\r\n");
	HMI_printf(FW_BANNER_FORMAT "freq band: %s\r\n", FW_VERSION, MBED_OS_VERSION, snmp_reset_reason_text(G_reset_reason), FREQ_BAND);
	HMI_printf("uptime: %lu s (%lu days %02lu:%02lu:%02lu)\r\n", up, up / 86400, (up / 3600) % 24, (up / 60) % 60, up % 60);
	HMI_printf("role: %s, callsign %s, radio %s, RF_power %i, temp %i degC\r\n", is_TDMA_master ? "master" : "client", CONF_radio_my_callsign + 2, CONF_radio_state_ON_OFF ? "ON" : "OFF", (int)CONF_radio_PA_PWR, G_temperature_SI4463);
	HMI_printf("last radio frame seen %u ms ago\r\n", (unsigned int)((now_us - last_rframe_seen) / 1000));
	if (!is_TDMA_master) {
		state = my_client_radio_connexion_state;
		signaling_get_counters(&since_request, &since_ack);
		TDMA_get_slave_slot(&alloc_age, &mf_ID, &mf_mask);
		HMI_printf("== connection (client) ==\r\n");
		HMI_printf("state %u (%s), own ID 0x%02X, rejection reason %u\r\n", state, state_text[(state < 6) ? state : 0], my_radio_client_ID, G_connect_rejection_reason);
		HMI_printf("periods since last request %i, since last ACK %i\r\n", since_request, since_ack);
		HMI_printf("TX slot: offset %u us, max burst %u us, alloc age %u, multiframe ID %u mask %u\r\n", offset_time_TX_slave, time_max_TX_burst, alloc_age, mf_ID, mf_mask);
		// the master top is stamped with the 24 bit radio timer, so compare modulo 2^24 (about 16.7 s)
		HMI_printf("last master top %u ms ago (modulo 16777 ms), TA %.1f km\r\n", (unsigned int)(((now_us - TDMA_slave_last_master_top) & 0xFFFFFF) / 1000), (my_radio_client_ID < radio_addr_table_size) ? 0.15 * (float)TDMA_table_TA[my_radio_client_ID] : 0.0);
		if (state == 2) { // before the connection the filtered values still build up from 0
			HMI_printf("downlink RSSI %.1f dBm, ERR %.2f %%\r\n", ((float)G_downlink_RSSI / 256 / 2 - 136), ((float)G_downlink_BER) / 500);
		} else {
			HMI_printf("downlink RSSI n/a, ERR n/a (not connected)\r\n");
		}
		if ( (state == 2) && (my_radio_client_ID < radio_addr_table_size) ) {
			HMI_printf("uplink RSSI %.1f dBm, ERR %.2f %%\r\n", ((float)G_radio_addr_table_RSSI[my_radio_client_ID] / 2 - 136), ((float)G_radio_addr_table_BER[my_radio_client_ID]) / 500);
		} else {
			HMI_printf("uplink RSSI n/a, ERR n/a (not connected)\r\n");
		}
		HMI_printf("master callsign %s\r\n", CONF_radio_master_callsign + 2);
		IP_int2char(LAN_conf_applied.DHCP_range_start, IP_c);
		IP_int2char(LAN_conf_applied.LAN_modem_IP, IP_c + 4);
		HMI_printf("applied range %i.%i.%i.%i size %lu, modem IP %i.%i.%i.%i\r\n", IP_c[0], IP_c[1], IP_c[2], IP_c[3], LAN_conf_applied.DHCP_range_size, IP_c[4], IP_c[5], IP_c[6], IP_c[7]);
	} else {
		HMI_printf("== clients (master) ==\r\n");
		for (i = 0; i < radio_addr_table_size; i++) {
			if (CONF_radio_addr_table_status[i]) {
				IP_int2char(CONF_radio_addr_table_IP_begin[i], IP_c);
				HMI_printf("ID %u %s status %i IP %i.%i.%i.%i size %lu\r\n", i, CONF_radio_addr_table_callsign[i] + 2, (int)CONF_radio_addr_table_status[i], IP_c[0], IP_c[1], IP_c[2], IP_c[3], CONF_radio_addr_table_IP_size[i]);
				HMI_printf("  last ACK %u s ago, TA %.1f km, RSSI %.1f dBm, ERR %.2f %%\r\n", (now_us - CONF_radio_addr_table_date[i]) / 1000000, 0.15 * (float)(TDMA_table_TA[i] / 10), (float)(G_radio_addr_table_RSSI[i] >> 8) / 2 - 136, ((float)G_radio_addr_table_BER[i]) / 500);
				HMI_printf("  FEC ok %u fixed %u lost %u\r\n", G_rx_fec_client_ok[i], G_rx_fec_client_fixed[i], G_rx_fec_client_lost[i]);
			}
		}
	}
	HMI_printf("== counters ==\r\n");
	HMI_printf("RX_Eth_IPv4 %i, TX_radio_IPv4 %i, RX_radio_IPv4 %i\r\n", RX_Eth_IPv4_counter, TX_radio_IPv4_counter, RX_radio_IPv4_counter);
	HMI_printf("to Ethernet: sent %u, held for ARP %u, no destination %u\r\n", G_eth_tx_ok, G_eth_tx_wait, G_radio_unrouted);
	HMI_printf("FEC ok %u fixed %u lost %u, seg_gap %u\r\n", G_rx_fec_ok, G_rx_fec_fixed, G_rx_fec_lost, G_rx_seg_gap);
	HMI_printf("== phy ==\r\n");
	HMI_phy_lines();
	HMI_printf("== config ==\r\n");
	HMI_config_lines();
	HMI_printf("== end of debug ==\r\nready> ");
}

void HMI_display_static(void) {
	
}

// Handlers of the 'set <parameter> <value>' command, one per parameter. Each one checks its value,
// applies it and prints the answer including the next prompt.

static const char* HMI_DHCP_master_warning(void) {
	if ( (is_TDMA_master) && (LAN_conf_saved.DHCP_server_active == 1) ) {
		return " (warning, DHCP inhibited in master mode)";
	}
	return "";
}

// Common part of the IP address parameters that need a radio restart.
static void HMI_set_IP_value(char* value, unsigned long int* target) {
	unsigned long int temp_uint = HMI_str2IP(value);
	if (temp_uint != 0) {
		RADIO_off_if_necessary(1);
		*target = temp_uint;
		RADIO_restart_if_necessary(1, 0, 1);
	}
}

// Common part of the size parameters that need a radio restart.
static void HMI_set_size_value(char* value, unsigned long int* target) {
	unsigned long int temp_uint;
	int temp = sscanf (value, "%ld", &temp_uint);
	if ( (temp==1) && (temp_uint!=0) ) {
		RADIO_off_if_necessary(1);
		*target = temp_uint;
		RADIO_restart_if_necessary(1, 0, 1);
		HMI_printf("OK\r\nready> ");
	}
	else {
		HMI_printf("wrong value\r\nready> ");
	}
}

static void HMI_set_callsign(char* value) {
	if (strlen(value) > 13) { // the field CONF_radio_my_callsign holds 2 MAC bytes, 13 characters and the end
		HMI_printf("ERROR: callsign too long, maximum is 13 characters.\r\nready> ");
		return;
	}
	RADIO_off_if_necessary(1);
	strcpy (CONF_radio_my_callsign+2, value);
	CONF_radio_my_callsign[0] = CONF_modem_MAC[4];
	CONF_radio_my_callsign[1] = CONF_modem_MAC[5];
	CONF_radio_my_callsign[15] = 0;
	RADIO_restart_if_necessary(1, 0, 1);
	HMI_printf("new callsign '%s'\r\nready> ", CONF_radio_my_callsign+2);
}

static void HMI_set_is_master(char* value) {
	unsigned char temp_uchar = HMI_yes_no_2int(value);
	if ( (temp_uchar==0) || (temp_uchar==1) ) {
		RADIO_off_if_necessary(1);
		is_TDMA_master = (temp_uchar == 1);
		RADIO_restart_if_necessary(1, 0, 1);
		HMI_printf("Master '%s'%s\r\nready> ", value, HMI_DHCP_master_warning());
	}
}

static void HMI_set_telnet_active(char* value) {
	unsigned char temp_uchar = HMI_yes_no_2int(value);
	if ( (temp_uchar==0) || (temp_uchar==1) ) {
		if(is_telnet_opened) { HMI_exit(); }
		is_telnet_active = temp_uchar;
		HMI_printf("telnet active '%s'\r\nready> ", value);
	}
}

static void HMI_set_telnet_routed(char* value) {
	unsigned char temp_uchar = HMI_yes_no_2int(value);
	if ( (temp_uchar==0) || (temp_uchar==1) ) {
		is_telnet_routed = temp_uchar;
		HMI_printf("telnet routed '%s'\r\nready> ", value);
	}
}

static void HMI_set_DNS_active(char* value) {
	unsigned char temp_uchar = HMI_yes_no_2int(value);
	if ( (temp_uchar==0) || (temp_uchar==1) ) {
		RADIO_off_if_necessary(1);
		LAN_conf_saved.LAN_DNS_activ = temp_uchar;
		RADIO_restart_if_necessary(1, 0, 1);
		HMI_printf("DNS active '%s'\r\nready> ", value);
	}
}

static void HMI_set_def_route_active(char* value) {
	unsigned char temp_uchar = HMI_yes_no_2int(value);
	if ( (temp_uchar==0) || (temp_uchar==1) ) {
		RADIO_off_if_necessary(1);
		LAN_conf_saved.LAN_def_route_activ = temp_uchar;
		RADIO_restart_if_necessary(1, 0, 1);
		HMI_printf("default route active '%s'\r\nready> ", value);
	}
}

static void HMI_set_master_FDD(char* value) {
	if(strcmp(value,"no") == 0) {
		CONF_master_FDD = 0;
	}
	else if(strcmp(value,"down") == 0) {
		CONF_master_FDD = 1;
	}
	else if(strcmp(value,"up") == 0) {
		CONF_master_FDD = 2;
	}
	else {
		HMI_printf("  wrong value\r\n");
		HMI_printf("ready> ");
		return;
	}
	RADIO_off_if_necessary(1);
	RADIO_restart_if_necessary(1, 0, 1);
	HMI_printf("ready> ");
}

static void HMI_set_radio_on_at_start(char* value) {
	unsigned char temp_uchar = HMI_yes_no_2int(value);
	if ( (temp_uchar==0) || (temp_uchar==1) ) {
		CONF_radio_default_state_ON_OFF = temp_uchar;
		HMI_printf("radio_on_at_start '%s'\r\nready> ", value);
	}
}

static void HMI_set_DHCP_active(char* value) {
	unsigned char temp_uchar = HMI_yes_no_2int(value);
	if ( (temp_uchar==0) || (temp_uchar==1) ) {
		LAN_conf_saved.DHCP_server_active = temp_uchar;
		HMI_printf("DHCP_active: '%s'%s\r\nready> ", value, HMI_DHCP_master_warning());
	}
}

static void HMI_set_modem_IP(char* value) { HMI_set_IP_value(value, &LAN_conf_saved.LAN_modem_IP); }
static void HMI_set_netmask(char* value) { HMI_set_IP_value(value, &LAN_conf_saved.LAN_subnet_mask); }
static void HMI_set_def_route_val(char* value) { HMI_set_IP_value(value, &LAN_conf_saved.LAN_def_route); }
static void HMI_set_DNS_value(char* value) { HMI_set_IP_value(value, &LAN_conf_saved.LAN_DNS_value); }
static void HMI_set_IP_begin(char* value) { HMI_set_IP_value(value, &CONF_radio_IP_start); }
static void HMI_set_master_down_IP(char* value) { HMI_set_IP_value(value, &CONF_master_down_IP); }
static void HMI_set_master_IP_size(char* value) { HMI_set_size_value(value, &CONF_radio_IP_size); }
static void HMI_set_client_req_size(char* value) { HMI_set_size_value(value, &CONF_radio_IP_size_requested); }

static void HMI_set_frequency(char* value) {
	float frequency;
	int temp = sscanf (value, "%f", &frequency);
	if ( (temp == 1) && (frequency<=FREQ_RANGE_MAX) && (frequency>FREQ_RANGE_MIN) ) {
		RADIO_off_if_necessary(0);
		frequency = (frequency - FREQ_RANGE_MIN)*1000 + 0.3; 
		CONF_frequency_HD = (short int)frequency;
		RADIO_restart_if_necessary(0, 1, 1);
		HMI_printf("OK\r\nready> ");
	} else {
		HMI_printf("ERROR: invalid value for frequency.\r\nready> ");
	}
}

static void HMI_set_freq_shift(char* value) {
	float frequency;
	int temp = sscanf (value, "%f", &frequency);
	if ( (temp == 1) && (frequency<=10) && (frequency>=-10) ) {
		RADIO_off_if_necessary(0);
		frequency = (frequency*1000);
		CONF_freq_shift = (short int)frequency;
		RADIO_restart_if_necessary(0, 1, 1);
		HMI_printf("OK\r\nready> ");
	} else {
		HMI_printf("ERROR: invalid value for freq_shift.\r\nready> ");
	}
}

static void HMI_set_RF_power(char* value) {
	int temp;
	if ( (sscanf (value, "%i", &temp) == 1) && (temp<128) ) {
		RADIO_off_if_necessary(0);
		CONF_radio_PA_PWR = temp;
		SI4463_set_power(G_SI4463);
		RADIO_restart_if_necessary(0, 0, 1);
		HMI_printf("OK\r\nready> ");
	} else {
		HMI_printf("ERROR: maximum RF_power value is 127.\r\nready> ");
	}
}

static void HMI_set_Eth_mode(char* value) {
	int temp;
	if ( (sscanf (value, "%i", &temp) == 1) && (temp >= 0) && (temp <= 7) ) {
		CONF_Eth_mode = temp;
		HMI_printf("OK\r\nready> ");
	} else {
		HMI_printf("error : max Eth_mode value 7\r\nready> ");
	}
}

static void HMI_set_modulation(char* value) {
	int temp;
	int found = sscanf (value, "%i", &temp);
	unsigned char temp_uchar = temp;
	if ( (found == 1) && ( ((temp_uchar>=11)&&(temp_uchar<=14)) || ((temp_uchar>=20)&&(temp_uchar<=24)) ) ) {
		RADIO_off_if_necessary(1);
		CONF_radio_modulation = temp_uchar;
		RADIO_restart_if_necessary(1, 1, 1);
		HMI_printf("OK\r\nready> ");
	} else {
		HMI_printf("ERROR: invalid value for modulation.\r\nready> ");
	}
}

static void HMI_set_radio_netw_ID(char* value) {
	int temp;
	int found = sscanf (value, "%i", &temp);
	unsigned char temp_uchar = temp;
	if ( (found == 1) && (temp_uchar <= 15) ) {
		RADIO_off_if_necessary(1);
		CONF_radio_network_ID = temp_uchar;
		RADIO_restart_if_necessary(1, 1, 1);
		HMI_printf("OK\r\nready> ");
	} else {
		HMI_printf("ERROR: invalid network ID, maximum is 15.\r\nready> ");
	}
}

typedef struct {
	const char* name;
	void (*handler)(char* value);
} HMI_set_entry_T;

static const HMI_set_entry_T HMI_set_table[] = {
	{"callsign", HMI_set_callsign},
	{"is_master", HMI_set_is_master},
	{"telnet_active", HMI_set_telnet_active},
	{"telnet_routed", HMI_set_telnet_routed},
	{"DNS_active", HMI_set_DNS_active},
	{"def_route_active", HMI_set_def_route_active},
	{"master_FDD", HMI_set_master_FDD},
	{"radio_on_at_start", HMI_set_radio_on_at_start},
	{"DHCP_active", HMI_set_DHCP_active},
	{"modem_IP", HMI_set_modem_IP},
	{"netmask", HMI_set_netmask},
	{"def_route_val", HMI_set_def_route_val},
	{"DNS_value", HMI_set_DNS_value},
	{"IP_begin", HMI_set_IP_begin},
	{"master_down_IP", HMI_set_master_down_IP},
	{"master_IP_size", HMI_set_master_IP_size},
	{"client_req_size", HMI_set_client_req_size},
	{"frequency", HMI_set_frequency},
	{"freq_shift", HMI_set_freq_shift},
	{"RF_power", HMI_set_RF_power},
	{"Eth_mode", HMI_set_Eth_mode},
	{"modulation", HMI_set_modulation},
	{"radio_netw_ID", HMI_set_radio_netw_ID},
};

void HMI_set_command(char* loc_param1, char* loc_param2) {
	if ((loc_param1) && (loc_param2)) {
		for (unsigned int i = 0; i < sizeof(HMI_set_table)/sizeof(HMI_set_table[0]); i++) {
			if (strcmp(loc_param1, HMI_set_table[i].name) == 0) {
				HMI_set_table[i].handler(loc_param2);
				return;
			}
		}
		HMI_printf("ERROR: unknown config parameter, type 'help' for the list.\r\nready> ");
	} else {
		HMI_printf("ERROR: 'set' needs a parameter and a value, for example: set RF_power 6\r\nready> ");
	}
}

unsigned long int HMI_str2IP(char* raw_string) {
	unsigned int IP_char_t[6];
	unsigned char IP_char[6]; 
	unsigned long int answer;
	int i;
	answer = sscanf(raw_string, "%i.%i.%i.%i", IP_char_t, IP_char_t+1, IP_char_t+2, IP_char_t+3);
	for (i=0;i<4; i++) {
		IP_char[i] = IP_char_t[i];
	}
	if (answer == 4) {
		answer = IP_char2int(IP_char);
		HMI_printf("OK\r\nready> ");
	} else {
		HMI_printf("bad IP format\r\nready> ");
		answer = 0;
	}
	return answer;
}

unsigned char HMI_yes_no_2int(char* raw_string) {
	unsigned char answer;
	if (strcmp (raw_string, "yes") == 0) {
		answer = 1;
	} 
	else if (strcmp (raw_string, "no") == 0) {
		answer = 0;
	}
	else {
		HMI_printf("value must be 'yes' or 'no'\r\nready> ");
		answer = -1;
	}
	return answer;
}

void HMI_print_who(void) {
	int i;
	unsigned int loc_age;
	unsigned int timer_snapshot;
	unsigned long int last_IP;
	float loc_RSSI, loc_TA;
	unsigned char IP_c[6];
	char temp_string[50] = {0x1B,0x5B,0x41,0x1B,0x5B,0x41,0x1B,0x5B,0x41,0x1B, 0x5B, 0x41,0x1B, 0x5B, 0x41,0x1B, 0x5B, 0x41,0x1B, 0x5B, 0x41,0x1B, 0x5B, 0x41,0x1B, 0x5B, 0x41,0x1B, 0x5B, 0x41,0x1B, 0x5B, 0x41,0x00};

	if (slow_counter == 0) { temp_string[0] = 0; }
	HMI_printf ("%s%i Master: ID:127 Callsign:%s\r\n", temp_string, slow_counter, CONF_radio_master_callsign+2);
	IP_int2char (LAN_conf_applied.LAN_modem_IP, IP_c);
	HMI_printf ("ME: Callsign:%s ID:%i modem IP:%i.%i.%i.%i\r\n", CONF_radio_my_callsign+2, my_radio_client_ID, IP_c[0], IP_c[1], IP_c[2], IP_c[3]);
	HMI_printf ("Clients:\r\n");
	timer_snapshot = GLOBAL_timer.read_us();
	for (i=0; i<radio_addr_table_size; i++) {
		
	}
	for (i=0; i<radio_addr_table_size; i++) {
		loc_age = timer_snapshot - CONF_radio_addr_table_date[i];
		loc_age = loc_age / 1000000;
		if (is_TDMA_master) {loc_age = 0;} // master : already timeout in state machine
		//printf ("age:%i ", loc_age);
		if ( (CONF_radio_addr_table_status[i]) && (loc_age < connexion_timeout) ) {
			HMI_printf (" ID:%i Callsign:%s ", i, CONF_radio_addr_table_callsign[i]+2);
			IP_int2char (CONF_radio_addr_table_IP_begin[i], IP_c);
			HMI_printf ("IP start:%i.%i.%i.%i ", IP_c[0], IP_c[1], IP_c[2], IP_c[3]);
			last_IP = CONF_radio_addr_table_IP_begin[i] + CONF_radio_addr_table_IP_size[i] - 1;
			IP_int2char (last_IP, IP_c);
			HMI_printf ("IP end:%i.%i.%i.%i ", IP_c[0], IP_c[1], IP_c[2], IP_c[3]);
			// The master keeps the RSSI in the upper byte and the distance ten times finer than
			// what it sends to the clients in the who message, a client stores both as received.
			if (is_TDMA_master) {
				loc_RSSI = (float)(G_radio_addr_table_RSSI[i] >> 8);
				loc_TA = (float)(TDMA_table_TA[i] / 10);
			} else {
				loc_RSSI = (float)G_radio_addr_table_RSSI[i];
				loc_TA = (float)TDMA_table_TA[i];
			}
			if (loc_RSSI > 0) {
				HMI_printf ("TA:%.1fkm RSSI:%.1fdBm ERR:%.2f%% ", 0.15*loc_TA, loc_RSSI/2-136, ((float)G_radio_addr_table_BER[i])/500);
			} else { // no measurement yet, a raw 0 would read as -136 dBm
				HMI_printf ("TA:%.1fkm RSSI:n/a ERR:%.2f%% ", 0.15*loc_TA, ((float)G_radio_addr_table_BER[i])/500);
			}
			if (is_TDMA_master) { // frames received from this client: ok / repaired / lost
				HMI_printf ("FEC ok:%u fixed:%u lost:%u\x1b[K\r\n", G_rx_fec_client_ok[i], G_rx_fec_client_fixed[i], G_rx_fec_client_lost[i]);
			} else {
				HMI_printf ("\x1b[K\r\n");
			}
		} else {
			HMI_printf ("                                                            \x1b[K\r\n");
		}
		
	}
	HMI_printf("CTRL+c to exit...\r\n");
}

void HMI_print_status(void) {
	static char text_radio_status[6][22] = { // indexed by my_client_radio_connexion_state
		"unknown",
		"waiting connection",
		"connected",
		"connection rejected",
		"disconnecting",
		"disconnected"
	};
	static char text_reject_reason[2][15] = {
		"IP requested",
		"clients",
	};
	char temp_string[22];
	float loc_downlink_bw;
	float loc_uplink_bw;
	int TA_loc; 
	temp_string[0]=0x1B;
	temp_string[1]=0x5B;
	temp_string[2]=0x41;
	temp_string[3]=0x1B;
	temp_string[4]=0x5B;
	temp_string[5]=0x41;
	temp_string[6]=0x1B;
	temp_string[7]=0x5B;
	temp_string[8]=0x41;
	temp_string[9]=0x1B;
	temp_string[10]=0x5B;
	temp_string[11]=0x41;
	temp_string[12]=0x1B;
	temp_string[13]=0x5B;
	temp_string[14]=0x41;
	temp_string[15]=0x1B;
	temp_string[16]=0x5B;
	temp_string[17]=0x41;
	temp_string[18]=0x00;
	//temp_string[15]=0x1B;
	//temp_string[16]=0x5B;
	//temp_string[17]=0x41;
	//temp_string[18]=0x00;
	if (slow_counter == 0) { temp_string[0] = 0; }
	if (is_TDMA_master) {
		loc_downlink_bw = G_uplink_bandwidth_temp * 0.004;
		loc_uplink_bw = G_downlink_bandwidth_temp * 0.004;
		TA_loc = 0;
	} else {
		loc_downlink_bw = G_downlink_bandwidth_temp * 0.004;
		loc_uplink_bw =  G_uplink_bandwidth_temp * 0.004;
		// before the connection the client ID is not a table index (0x7E)
		TA_loc = (my_radio_client_ID < radio_addr_table_size) ? TDMA_table_TA[my_radio_client_ID] : 0;
	}
	if (CONF_radio_state_ON_OFF == 0) {
		HMI_printf("%s   %i status: radio OFF               \r\n", temp_string, slow_counter);
	}
	else if (my_client_radio_connexion_state == 0x03) {
		if ( (G_connect_rejection_reason == 2) || (G_connect_rejection_reason == 3) ) { // the reason comes from the master
			HMI_printf("%s   %i status: rejected because too many %s \r\n", temp_string, slow_counter, text_reject_reason[G_connect_rejection_reason-2]);
		} else {
			HMI_printf("%s   %i status: rejected, reason code %i \r\n", temp_string, slow_counter, G_connect_rejection_reason);
		}
	} else {
		HMI_printf("%s   %i status: %s TA:%.1fkm Temp:%idegC   \r\n", temp_string, slow_counter, text_radio_status[(my_client_radio_connexion_state < 6) ? my_client_radio_connexion_state : 0], 0.15*(float)TA_loc, G_temperature_SI4463);
	}
	//HMI_printf("   TX buff filling %i\r\n", (TX_buff_ext_WR_pointer - TX_buff_ext_RD_pointer)*128);//!!!test
	if ( (is_TDMA_master) && (CONF_master_FDD == 2) ) {
		HMI_printf("   RX tops from master FDD down %i\r\n", RX_top_FDD_up_counter);
	} else {
		HMI_printf("   RX_Eth_IPv4 %i ;TX_radio_IPv4 %i ; RX_radio_IPv4 %i   \r\n", RX_Eth_IPv4_counter, TX_radio_IPv4_counter, RX_radio_IPv4_counter);
		//HMI_printf("   RX_Eth_IPv4 %i ;TX_radio_IPv4 %i ; RX_radio_IPv4 %i   \r\n", RX_Eth_IPv4_counter, TX_radio_IPv4_counter, (TX_buff_ext_WR_pointer - TX_buff_ext_RD_pointer)*128);//!!!! debug FIFO filling
	}
	HMI_printf("   RX frames FEC ok:%u fixed:%u lost:%u seg_gap:%u   \r\n", G_rx_fec_ok, G_rx_fec_fixed, G_rx_fec_lost, G_rx_seg_gap);
	if ( (!is_TDMA_master) && (RSSI_stat_pkt_nb > 0) ) {
		//HMI_printf("RSSI: %i\r\nCTRL+c to exit...\r\n", (RSSI_total_stat / RSSI_stat_pkt_nb) );
		HMI_printf("   DOWNLINK - bandwidth:%.1f RSSI:%.1f ERR:%.2f%%    \r\n", loc_downlink_bw, ((float)G_downlink_RSSI/256/2-136), ((float)G_downlink_BER)/500); // /500
		RSSI_total_stat = 0;
		RSSI_stat_pkt_nb = 0;
	} else {
		HMI_printf("   DOWNLINK - bandwidth: %.1f RSSI:       ERR:      \r\n", loc_downlink_bw);
		
	}
	if ( (!is_TDMA_master) && (my_client_radio_connexion_state == 2) && (my_radio_client_ID < radio_addr_table_size) ) {
		HMI_printf("   UPLINK -   bandwidth:%.1f RSSI:%.1f ERR:%.2f%%    \r\nCTRL+c to exit...\r\n", loc_uplink_bw, ((float)G_radio_addr_table_RSSI[my_radio_client_ID]/2-136), ((float)G_radio_addr_table_BER[my_radio_client_ID])/500);
	} else {
		HMI_printf("   UPLINK -   bandwidth:%.1f RSSI:     ERR:      \r\nCTRL+c to exit...\r\n", loc_uplink_bw);
	}
	G_downlink_bandwidth_temp = 0;
	G_uplink_bandwidth_temp = 0;
}

void HMI_periodic_call (void) {
	if (display_status_ongoing) {
		HMI_print_status();
		slow_counter++;
	}
	if (display_who_ongoing) {
		HMI_print_who();
		slow_counter++;
	}
}

void HMI_printf_detail (void) {
	int size;
	if (is_telnet_opened) {
		size = strlen (HMI_out_str);
		W5500_write_TX_buffer (W5500_p1, TELNET_SOCKET, (unsigned char*)HMI_out_str, size, 0);
	}
	else {
		printf("%s", HMI_out_str);
		fflush(stdout);
	}
}
