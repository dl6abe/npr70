// Host stub for mbed.h: only what W5500.h and the extracted firmware code need.
#ifndef STUB_MBED_H
#define STUB_MBED_H
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <chrono>
using namespace std::chrono_literals;
static int stub_timer_fixed = 0;           // a test sets this to drive the time itself
static unsigned int stub_timer_us = 0;
struct Timer {
	unsigned int read_us() const { static unsigned int t = 0; if (stub_timer_fixed) return stub_timer_us; return t += 1000; }
	std::chrono::microseconds elapsed_time() const { return std::chrono::microseconds(read_us()); }
};
struct SPI { int write(const char* tx, int tx_len, char* rx, int rx_len); };
struct DigitalOut { void write(int) {} };
struct DigitalIn { int v; DigitalIn() : v(0) {} int read() { return v; } operator int() { return v; } };
static inline void wait_us(int) {}
namespace ThisThread { static inline void sleep_for(std::chrono::milliseconds) {} }
static inline void wait(int) {}
#endif
