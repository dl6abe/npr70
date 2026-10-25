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

#include "mbed.h"
#include "W5500.h"
#include "SI4463.h"
#include "HMI_telnet.h"
#include "SNMP.h"
#include "Eth_IPv4.h"
#include "DHCP_ARP.h"
#include "global_variables.h"
#include "L1L2_radio.h"
#include "TDMA.h"
#include "signaling.h"
#include "config_flash.h"
#include "diag.h"

#include "ext_SRAM2.h"

//#define NPR_L476

DigitalInOut FDD_trig_pin(PA_10);//GPIO_11
InterruptIn FDD_trig_IRQ(PA_10);//GPIO_11
DigitalOut PTT_PA_pin(PA_9);//GPIO_10

DigitalOut SI4463_SDN(PA_1);

AnalogIn Random_pin(PA_0);
DigitalOut LED_RX_loc(PB_1);
DigitalOut LED_connected(PA_12);

DigitalIn Int_W5500(PA_8);
DigitalOut CS1(PA_11);//CS W5500
// W5500 and external SRAM on SPI3 (AF6), radio on SPI1 (AF5): two different peripherals.
// PB_5/PB_4/PB_3 can be SPI1 (AF5) or SPI3 (AF6). The plain names select SPI1 (the same peripheral as the radio
// on PA_7/PA_6/PA_5) and the _ALT0 names select SPI3. Two SPI objects on one peripheral corrupt each other.
SPI_F4HDK spi_2(PB_5_ALT0, PB_4_ALT0, PB_3_ALT0); // mosi, miso, sclk
DigitalOut CS3(PB_0);// CS ext SRAM PB_0 

InterruptIn Int_SI4463(PA_3);
DigitalOut CS2(PA_4);
SPI_F4HDK spi_1(PA_7, PA_6, PA_5); // mosi, miso, sclk

// Translate the reset flags of the MCU into the code behind G_reset_reason.
// One reset sets several flags (a power-on also sets the pin flag), so the order matters.
static unsigned char decode_reset_reason(uint32_t csr) {
	if (csr & RCC_CSR_LPWRRSTF) return 6;
	if (csr & RCC_CSR_WWDGRSTF) return 5;
	if (csr & RCC_CSR_IWDGRSTF) return 4;
	if (csr & RCC_CSR_SFTRSTF) return 3;
	if (csr & RCC_CSR_OBLRSTF) return 7;
	if (csr & RCC_CSR_BORRSTF) return 1;
	if (csr & RCC_CSR_PINRSTF) return 2;
	return 0;
}

int main()
{
	DIAG_init();
	// read the reset flags first and clear them so the next start sees only its own reason
	G_reset_reason = decode_reset_reason(RCC->CSR);
	RCC->CSR |= RCC_CSR_RMVF;
    ThisThread::sleep_for(200ms);
	pc.baud(921600);
    pc.printf("\r\n\r\n" FW_BANNER_FORMAT, FW_VERSION, MBED_OS_VERSION, snmp_reset_reason_text(G_reset_reason));
	
	Timer slow_timer; 
	int i = 1;
	int temperature_timer = 0;
	
	static LAN_conf_T* LAN_conf_p;
	LAN_conf_p = &LAN_conf_applied;
	
	static W5500_chip W5500_1;
	W5500_p1 = &W5500_1;
	W5500_1.spi_port = &spi_2;
    W5500_1.cs = &CS1;
    W5500_1.interrupt = &Int_W5500; 
	
	static ext_SRAM_chip SPI_SRAM;
	SPI_SRAM_p = &SPI_SRAM;
	SPI_SRAM.spi_port = &spi_2;
	SPI_SRAM.cs = &CS3;
	
	static SI4463_Chip SI4463_1;
	SI4463_1.spi = &spi_1;//1
	SI4463_1.cs = &CS2;//2
	SI4463_1.interrupt = &Int_SI4463;
	SI4463_1.RX_LED = &LED_RX_loc;
	SI4463_1.SDN = &SI4463_SDN;
	
	G_SI4463 = &SI4463_1;
	
	G_FDD_trig_pin = &FDD_trig_pin;
	G_FDD_trig_IRQ = &FDD_trig_IRQ;
	G_PTT_PA_pin = &PTT_PA_pin;
	
	reset_DHCP_table(LAN_conf_p);

	snmp_init();
		
    spi_2.format(8,0);
	spi_1.format(8,0);
	spi_2.frequency(20000000); 
	spi_1.frequency(10000000);
	
	for(i=0; i<radio_addr_table_size; i++) {
		CONF_radio_addr_table_status[i] = 0;
	}
	
	G_FDD_trig_pin->input();
	G_PTT_PA_pin->write(0);
	LED_RX_loc = 0;
	LED_connected = 0;
    CS1=1;
	CS2=1;
	CS3=1;
	SI4463_SDN = 1;
	
	ThisThread::sleep_for(20ms);
	is_SRAM_ext = ext_SRAM_detect();
	
	LED_RX_loc = 1; 
	for (i=0; i<7; i++) {
		ThisThread::sleep_for(200ms);
		LED_RX_loc = !LED_RX_loc;
		LED_connected = !LED_connected;
		SI4463_SDN = !SI4463_SDN;
	}
	LED_RX_loc = 0;
	LED_connected = 0;
	SI4463_SDN = 1;
	
	printf("\r\n");

	RX_FIFO_WR_point = 0;
	RX_FIFO_RD_point = 0;
	RX_FIFO_last_received = 0;


	GLOBAL_timer.reset();
	GLOBAL_timer.start();
	TDMA_init_all();
	
	for (i=0; i<0x2000; i++) {
		RX_FIFO_data[i] = 0;
	}

	printf("      \r\n");
	for (i=0; i<radio_addr_table_size; i++) {
		CONF_radio_addr_table_IP_begin[i] = 0; // 0 = entry not active
		
	}
	if (is_SRAM_ext) {
		printf("config WITH ext SRAM\r\n");
	} else {
		printf("config WITHOUT ext SRAM\r\n");
	}
	
	ThisThread::sleep_for(100ms);
	NFPR_config_read(&Random_pin);

	SI4463_get_state(G_SI4463);

	i = SI4463_configure_all();
	if (i == 1) {
		HMI_printf("SI4463 configured\r\n");
	} else {
		HMI_printf("SI4463 error while configure\r\n");
		SI4463_print_version(G_SI4463);
		ThisThread::sleep_for(5000ms);
		if (serial_term_loop() == 0) {//no serial char detected
			NVIC_SystemReset(); //reboot
		}
	}
	
	W5500_initial_configure(W5500_p1);
	ThisThread::sleep_for(2ms);
#ifdef EXT_SRAM_USAGE
	ext_SRAM_set_mode(SPI_SRAM_p);
	ThisThread::sleep_for(2ms);
	ext_SRAM_init();
#endif
	TDMA_NULL_frame_init(70);
	
	SI4463_periodic_temperature_check(G_SI4463);
	Int_SI4463.fall(&SI4463_HW_interrupt);
	if (CONF_radio_default_state_ON_OFF) {
		RADIO_on(1, 0, 1);//init state, no reconfigure, HMI output
	}
	HMI_printf("ready> ");
	slow_timer.start();

	unsigned int timer_snapshot;
	int slow_action_counter = 0;
	int signaling_counter = 0;
	
	int telnet_counter = 0;
	
	// There is no real time operating system on the modem, but rather an infinite loop "Arduino style"
	// that runs the various functions of the modem.
	while(1) {	
		for (i=0; i<100; i++) {
			if ( (is_TDMA_master) && (CONF_master_FDD == 2) ) {
				FDDup_RX_FIFO_dequeue();
			} else {
				radio_RX_FIFO_dequeue(W5500_p1);
			}
			Eth_RX_dequeue(W5500_p1); 
			TDMA_slave_timeout();
#ifdef EXT_SRAM_USAGE
			ext_SRAM_periodic_call();
#endif
			if (is_SRAM_ext == 1) {
				ext_SRAM_periodic_call();
			}
			timer_snapshot = TIMER_READ_US_SLOW(slow_timer);
			if (timer_snapshot > 666000) {//666000
				slow_timer.reset();
				slow_action_counter++;
				if (slow_action_counter > 2) {slow_action_counter = 0; }
				
				if (slow_action_counter == 0) {
					HMI_periodic_call();
				}

				
				if (slow_action_counter == 1) {//every 2 sec
					signaling_counter++;
					if (signaling_counter >= CONF_signaling_period) {
						signaling_periodic_call();
						signaling_counter = 0;
					}
				}
				
				if (slow_action_counter == 2) {
					DHCP_ARP_periodic_free_table();
					W5500_re_configure_periodic_call(W5500_p1);
				
					temperature_timer++;
					if(temperature_timer > 15) {// 15 every 30 sec
						temperature_timer = 0;
						G_need_temperature_check = 1;
						SI4463_periodic_temperature_check_2();//SI4463_periodic_temperature_check(G_SI4463);
					}
				}
			}
		}
		
		if (is_TDMA_master) {
			if (CONF_radio_state_ON_OFF==1) {LED_connected.write((timer_snapshot >> 19) & 1);}
			else {LED_connected.write(0);}
		} else {
			if (my_client_radio_connexion_state == 2) {
				LED_connected.write(1);
			}else{
				LED_connected.write(0);
			}
		}
		if (is_telnet_active) {
			
			telnet_counter++;
			if (telnet_counter>10) {
				telnet_loop(W5500_p1);
				telnet_counter = 0;
			}
		}
		serial_term_loop();

		snmp_loop(W5500_p1);
		W5500_link_poll(W5500_p1);
		
		if ( (LAN_conf_applied.DHCP_server_active == 1) && (!is_TDMA_master) ) {
			DHCP_server(LAN_conf_p, W5500_p1);
		}
	}
}
