unsigned long int IP_char2int(unsigned char* IP_char) {
	unsigned long int result;
	result = (0x1000000UL*IP_char[0]) + (0x10000UL*IP_char[1]) + (0x100UL*IP_char[2]) + IP_char[3]; // unsigned, a first byte of 128 or more overflows int
	return result;
}
