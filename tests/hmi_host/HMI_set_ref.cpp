// Original 'set' command code of commit 23fbcc8, frozen as reference for run.sh.
#include "HMI_telnet.h"
#include "global_variables.h"
#include "Eth_IPv4.h"
#include "SI4463.h"
void HMI_set_command(char* loc_param1, char* loc_param2) {
	int temp;
	unsigned char temp_uchar;
	unsigned long int temp_uint;
	float frequency;
	// unsigned char previous_freq_band;
	char DHCP_warning[50];
	if ((loc_param1) && (loc_param2)) {
		if (strcmp(loc_param1, "callsign") == 0) {
			if (strlen(loc_param2) > 13) { // the field CONF_radio_my_callsign holds 2 MAC bytes, 13 characters and the end
				HMI_printf("ERROR: callsign too long, maximum is 13 characters.\r\nready> ");
				return;
			}
			RADIO_off_if_necessary(1);
			strcpy (CONF_radio_my_callsign+2, loc_param2);
			CONF_radio_my_callsign[0] = CONF_modem_MAC[4];
			CONF_radio_my_callsign[1] = CONF_modem_MAC[5];
			CONF_radio_my_callsign[15] = 0;
			RADIO_restart_if_necessary(1, 0, 1);
			HMI_printf("new callsign '%s'\r\nready> ", CONF_radio_my_callsign+2);
		} 
		else if (strcmp(loc_param1, "is_master") == 0) {
			temp_uchar = HMI_yes_no_2int(loc_param2);
			if ( (temp_uchar==0) || (temp_uchar==1) ) {
				RADIO_off_if_necessary(1);
				is_TDMA_master = (temp_uchar == 1);
				RADIO_restart_if_necessary(1, 0, 1);
				if ( (is_TDMA_master) && (LAN_conf_saved.DHCP_server_active == 1) ) {
					strcpy (DHCP_warning, " (warning, DHCP inhibited in master mode)"); 
				} else {
					strcpy (DHCP_warning, ""); 
				}
				HMI_printf("Master '%s'%s\r\nready> ", loc_param2, DHCP_warning);
			}
		}
		else if (strcmp(loc_param1, "telnet_active") == 0) {
			temp_uchar = HMI_yes_no_2int(loc_param2);
			if ( (temp_uchar==0) || (temp_uchar==1) ) {
				if(is_telnet_opened) { HMI_exit(); }
				is_telnet_active = temp_uchar;
				HMI_printf("telnet active '%s'\r\nready> ", loc_param2);
			}
		}
		else if (strcmp(loc_param1, "telnet_routed") == 0) {
			temp_uchar = HMI_yes_no_2int(loc_param2);
			if ( (temp_uchar==0) || (temp_uchar==1) ) {
				is_telnet_routed = temp_uchar;
				//W5500_re_configure_gateway(W5500_p1);
				HMI_printf("telnet routed '%s'\r\nready> ", loc_param2);
			}
		}
		else if (strcmp(loc_param1, "DNS_active") == 0) {
			temp_uchar = HMI_yes_no_2int(loc_param2);
			if ( (temp_uchar==0) || (temp_uchar==1) ) {
				RADIO_off_if_necessary(1);
				LAN_conf_saved.LAN_DNS_activ = temp_uchar;
				RADIO_restart_if_necessary(1, 0, 1);
				HMI_printf("DNS active '%s'\r\nready> ", loc_param2);
			}
		}
		else if (strcmp(loc_param1, "def_route_active") == 0) {
			temp_uchar = HMI_yes_no_2int(loc_param2);
			if ( (temp_uchar==0) || (temp_uchar==1) ) {
				RADIO_off_if_necessary(1);
				LAN_conf_saved.LAN_def_route_activ = temp_uchar;
				//W5500_re_configure_gateway(W5500_p1);
				RADIO_restart_if_necessary(1, 0, 1);
				HMI_printf("default route active '%s'\r\nready> ", loc_param2);
			}
		}
		else if (strcmp(loc_param1, "master_FDD") == 0) {
			if(strcmp(loc_param2,"no") == 0) {
				CONF_master_FDD = 0;
				RADIO_off_if_necessary(1);
				RADIO_restart_if_necessary(1, 0, 1);
			}
			else if(strcmp(loc_param2,"down") == 0) {
				CONF_master_FDD = 1;
				RADIO_off_if_necessary(1);
				RADIO_restart_if_necessary(1, 0, 1);
			}
			else if(strcmp(loc_param2,"up") == 0) {
				CONF_master_FDD = 2;
				RADIO_off_if_necessary(1);
				RADIO_restart_if_necessary(1, 0, 1);
			}
			else {
				HMI_printf("  wrong value\r\n");
			}
			HMI_printf("ready> ");
		}
		//else if (strcmp(loc_param1, "trans_method") == 0) {
		//	if(strcmp(loc_param2,"IP") == 0) {
		//		CONF_transmission_method = 0;
		//		RADIO_off_if_necessary(1);
		//		RADIO_restart_if_necessary(1, 0, 1);
		//	}
		//	else if(strcmp(loc_param2,"Eth") == 0) {
		//		CONF_transmission_method = 1;
		//		RADIO_off_if_necessary(1);
		//		RADIO_restart_if_necessary(1, 0, 1);
		//	}
		//	else {
		//		HMI_printf("  wrong value\r\n");
		//	}
		//	HMI_printf("ready> ");
		//}
		//else if (strcmp(loc_param1, "client_static_IP") == 0) {
		//	temp_uchar = HMI_yes_no_2int(loc_param2);
		//	if ( (temp_uchar==0) || (temp_uchar==1) ) {
		//		RADIO_off_if_necessary(1);
		//		CONF_radio_static_IP_requested = temp_uchar;
		//		HMI_printf("client static IP '%s'\r\nready> ", loc_param2);
		//		RADIO_restart_if_necessary(1, 0, 1);
		//	}
		//}
		else if (strcmp(loc_param1, "radio_on_at_start") == 0) {
			temp_uchar = HMI_yes_no_2int(loc_param2);
			if ( (temp_uchar==0) || (temp_uchar==1) ) {
				CONF_radio_default_state_ON_OFF = temp_uchar;
				HMI_printf("radio_on_at_start '%s'\r\nready> ", loc_param2);
			}
		}
		else if (strcmp(loc_param1, "DHCP_active") == 0) {
			temp_uchar = HMI_yes_no_2int(loc_param2);
			if ( (temp_uchar==0) || (temp_uchar==1) ) {
				LAN_conf_saved.DHCP_server_active = temp_uchar;
				if ( (is_TDMA_master) && (LAN_conf_saved.DHCP_server_active == 1) ) {
					strcpy (DHCP_warning, " (warning, DHCP inhibited in master mode)"); 
				} else {
					strcpy (DHCP_warning, ""); 
				}
				HMI_printf("DHCP_active: '%s'%s\r\nready> ", loc_param2, DHCP_warning);
			}
		}
		else if (strcmp(loc_param1, "modem_IP") == 0) {
			temp_uint = HMI_str2IP(loc_param2);
			if (temp_uint !=0) {
				RADIO_off_if_necessary(1);
				LAN_conf_saved.LAN_modem_IP = temp_uint;
				//HMI_force_exit();
				//W5500_re_configure();
				RADIO_restart_if_necessary(1, 0, 1);
			}
		}
		else if (strcmp(loc_param1, "netmask") == 0) {
			temp_uint = HMI_str2IP(loc_param2);
			if (temp_uint !=0) {
				RADIO_off_if_necessary(1);
				LAN_conf_saved.LAN_subnet_mask = temp_uint;
				//HMI_force_exit();
				//W5500_re_configure();
				RADIO_restart_if_necessary(1, 0, 1);
			}
		}
		else if (strcmp(loc_param1, "def_route_val") == 0) {
			temp_uint = HMI_str2IP(loc_param2);
			if (temp_uint !=0) {
				RADIO_off_if_necessary(1);
				LAN_conf_saved.LAN_def_route = temp_uint;
				//W5500_re_configure_gateway(W5500_p1);
				RADIO_restart_if_necessary(1, 0, 1);
			}
		}
		else if (strcmp(loc_param1, "DNS_value") == 0) {
			temp_uint = HMI_str2IP(loc_param2);
			if (temp_uint !=0) {
				RADIO_off_if_necessary(1);
				LAN_conf_saved.LAN_DNS_value = temp_uint;
				RADIO_restart_if_necessary(1, 0, 1);
			}
		}
		else if (strcmp(loc_param1, "IP_begin") == 0) {
			temp_uint = HMI_str2IP(loc_param2);
			if (temp_uint !=0) {
				RADIO_off_if_necessary(1);
				CONF_radio_IP_start = temp_uint;
				RADIO_restart_if_necessary(1, 0, 1);
			}
		}
		else if (strcmp(loc_param1, "master_down_IP") == 0) {
			temp_uint = HMI_str2IP(loc_param2);
			if (temp_uint !=0) {
				RADIO_off_if_necessary(1);
				CONF_master_down_IP = temp_uint;
				RADIO_restart_if_necessary(1, 0, 1);
			}
		}
		else if (strcmp(loc_param1, "master_IP_size") == 0) {
			temp = sscanf (loc_param2, "%ld", &temp_uint);
			if ( (temp==1) && (temp_uint!=0) ) {
				RADIO_off_if_necessary(1);
				CONF_radio_IP_size = temp_uint;
				RADIO_restart_if_necessary(1, 0, 1);
				HMI_printf("OK\r\nready> ");
			}
			else {
				HMI_printf("wrong value\r\nready> ");
			}
		}
		else if (strcmp(loc_param1, "client_req_size") == 0) {
			temp = sscanf (loc_param2, "%ld", &temp_uint);
			if ( (temp==1) && (temp_uint!=0) ) {
				RADIO_off_if_necessary(1);
				CONF_radio_IP_size_requested = temp_uint;
				RADIO_restart_if_necessary(1, 0, 1);
				HMI_printf("OK\r\nready> ");
			}
			else {
				HMI_printf("wrong value\r\nready> ");
			}
		}

		else if (strcmp(loc_param1, "frequency") == 0) {
			temp = sscanf (loc_param2, "%f", &frequency);
			if ( (temp == 1) && (frequency<=FREQ_RANGE_MAX) && (frequency>FREQ_RANGE_MIN) ) {
				RADIO_off_if_necessary(0);
				frequency = (frequency - FREQ_RANGE_MIN)*1000 + 0.3; 
				CONF_frequency_HD = (short int)frequency;
				//RADIO_compute_freq_params();//REMOVE TEST
				RADIO_restart_if_necessary(0, 1, 1);
				HMI_printf("OK\r\nready> ");
			} else {
				HMI_printf("ERROR: invalid value for frequency.\r\nready> ");
			}
		}
		else if (strcmp(loc_param1, "freq_shift") == 0) {
			temp = sscanf (loc_param2, "%f", &frequency);
			if ( (temp == 1) && (frequency<=10) && (frequency>=-10) ) {
				RADIO_off_if_necessary(0);
				frequency = (frequency*1000);
				CONF_freq_shift = (short int)frequency;
				//RADIO_compute_freq_params();//REMOVE TEST
				//if (CONF_frequency_band == previous_freq_band) {
				//	RADIO_restart_if_necessary(0, 0, 1);
				//}else {
				RADIO_restart_if_necessary(0, 1, 1);
				//}
				HMI_printf("OK\r\nready> ");
			} else {
				HMI_printf("ERROR: invalid value for freq_shift.\r\nready> ");
			}
		}
		else if (strcmp(loc_param1, "RF_power") == 0) {
			temp_uint = sscanf (loc_param2, "%i", &temp); 
			if ( (temp_uint == 1) && (temp<128) ) {
				RADIO_off_if_necessary(0);
				CONF_radio_PA_PWR = temp;
				SI4463_set_power(G_SI4463);
				RADIO_restart_if_necessary(0, 0, 1);
				HMI_printf("OK\r\nready> ");
			} else {
				HMI_printf("ERROR: maximum RF_power value is 127.\r\nready> ");
			}
		}
		else if (strcmp(loc_param1, "Eth_mode") == 0) {
			temp_uint = sscanf (loc_param2, "%i", &temp); 
			if ( (temp_uint == 1) && (temp >= 0) && (temp <= 7) ) {
				CONF_Eth_mode = temp;
				HMI_printf("OK\r\nready> ");
			} else {
				HMI_printf("error : max Eth_mode value 7\r\nready> ");
			}
		}
		else if (strcmp(loc_param1, "modulation") == 0) {
			temp_uint = sscanf (loc_param2, "%i", &temp);
			temp_uchar = temp;
			//if ( (temp_uint == 1) && ((temp_uchar==13)||(temp_uchar==14)||(temp_uchar==22)||(temp_uchar==23)||(temp_uchar==24)) ) {
			if ( (temp_uint == 1) && ( ((temp_uchar>=11)&&(temp_uchar<=14)) || ((temp_uchar>=20)&&(temp_uchar<=24)) ) ) {
				RADIO_off_if_necessary(1);
				CONF_radio_modulation = temp_uchar;
				RADIO_restart_if_necessary(1, 1, 1);
				HMI_printf("OK\r\nready> ");
			} else {
				HMI_printf("ERROR: invalid value for modulation.\r\nready> ");
			}
		}
		else if (strcmp(loc_param1, "radio_netw_ID") == 0) {
			temp_uint = sscanf (loc_param2, "%i", &temp);
			temp_uchar = temp;
			if ( (temp_uint == 1) && (temp_uchar <= 15) ) {
				RADIO_off_if_necessary(1);
				CONF_radio_network_ID = temp_uchar;
				RADIO_restart_if_necessary(1, 1, 1);
				HMI_printf("OK\r\nready> ");
			} else {
				HMI_printf("ERROR: invalid network ID, maximum is 15.\r\nready> ");
			}
		}
		else {
			HMI_printf("ERROR: unknown config parameter, type 'help' for the list.\r\nready> ");
		}
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
