// Host stub: HMI_printf collects the text in a buffer that driver.cpp prints.
#ifndef STUB_HMI_TELNET_H
#define STUB_HMI_TELNET_H
#include "mbed.h"
extern char HMI_out_str[1024];
void HMI_printf_detail (void);
#define HMI_printf(param, ...) sprintf (HMI_out_str, param, ##__VA_ARGS__);\
	HMI_printf_detail();
void HMI_exit(void);
unsigned long int HMI_str2IP(char* raw_string);
unsigned char HMI_yes_no_2int(char* raw_string);
void HMI_set_command(char* loc_param1, char* loc_param2);
#endif
