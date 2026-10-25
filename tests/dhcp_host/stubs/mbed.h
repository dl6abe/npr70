// Host stub for the parts of mbed.h that the DHCP code uses.
#ifndef STUB_MBED_H
#define STUB_MBED_H
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <chrono>
struct Timer {
	unsigned int read_us() const { static unsigned int t = 0; return t += 1000; }
	std::chrono::microseconds elapsed_time() const { return std::chrono::microseconds(read_us()); }
};
#endif
