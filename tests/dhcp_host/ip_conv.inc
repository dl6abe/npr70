unsigned long int IP_char2int(unsigned char* IP_char) {
	unsigned long int result;
	result = (0x1000000UL*IP_char[0]) + (0x10000UL*IP_char[1]) + (0x100UL*IP_char[2]) + IP_char[3]; // unsigned, a first byte of 128 or more overflows int
	return result;
}

void IP_int2char (unsigned long int IP_int, unsigned char* IP_char) {
	IP_char[0] = (IP_int & 0xFF000000) / 0x1000000;
	IP_char[1] = (IP_int & 0x00FF0000) / 0x10000;
	IP_char[2] = (IP_int & 0x0000FF00) / 0x100;
	IP_char[3] = (IP_int & 0xFF);
} 
