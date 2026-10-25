// Diagnosis commands of the console: 'stack' (stack and heap usage) and 'latency' (jitter of the 1 ms Ticker callback).
// The Ticker callback is the mechanism the TDMA timing uses to start a transmission (attach in SI4463.cpp and
// TDMA.cpp), so its lateness is the jitter of those start times. It includes every delay of the interrupt: other
// interrupts, sections with interrupts disabled. It is not the delay of the radio interrupt pin itself.
#include "mbed.h"
#include "hal/us_ticker_api.h"
#include "HMI_telnet.h"
#include "diag.h"
#include "diag_core.h"
#include "global_variables.h"
#include <malloc.h>

#if NPR_DIAG

#include "platform/mbed_stats.h"
extern "C" {
	extern unsigned char* mbed_stack_isr_start;
	extern uint32_t mbed_stack_isr_size;
	extern uint32_t mbed_heap_size_total;
}

#define DIAG_LAT_PERIOD_US 1000

static int isr_filled = 0;

void DIAG_init(void) {
	uint32_t* low = (uint32_t*)mbed_stack_isr_start;
	uint32_t msp = __get_MSP();
	uint32_t top = (uint32_t)mbed_stack_isr_start + mbed_stack_isr_size;
	if ( (low != NULL) && (msp > (uint32_t)low + 16) && (msp <= top) ) {
		core_util_critical_section_enter();     // no interrupt may use the stack while it is filled
		for (uint32_t* p = low; (uint32_t)p < msp - 16; p++) {
			*p = DIAG_STACK_PATTERN;
		}
		core_util_critical_section_exit();
		isr_filled = 1;
	}
}

// The lines of 'stack' without the prompt.
static void stack_lines(void) {
	char line[100];
	HMI_printf("stack use, highest values since the start (bytes):\r\n");
#if defined(MBED_THREAD_STATS_ENABLED)
	mbed_stats_thread_t threads[8];
	size_t n = mbed_stats_thread_get_each(threads, 8);
	for (size_t i = 0; i < n; i++) {
		char label[40];
		snprintf(label, sizeof(label), "thread %s", threads[i].name ? threads[i].name : "?");
		diag_stack_line(label, threads[i].stack_size - threads[i].stack_space, threads[i].stack_size, line, sizeof(line));
		HMI_printf("%s", line);
	}
#else
	HMI_printf("  thread statistics are not built in\r\n");
#endif
	if (isr_filled) {
		diag_stack_line("interrupt stack", diag_stack_used_bytes((const uint32_t*)mbed_stack_isr_start, mbed_stack_isr_size / 4), mbed_stack_isr_size, line, sizeof(line));
		HMI_printf("%s", line);
	} else {
		HMI_printf("  interrupt stack: not measured\r\n");
	}
	struct mallinfo mi = mallinfo();
	HMI_printf("heap: %u bytes in use, %u taken so far of the heap area of %u bytes\r\n", (unsigned int)mi.uordblks, (unsigned int)mi.arena, (unsigned int)mbed_heap_size_total);
}

void DIAG_stack_print(void) {
	stack_lines();
	HMI_printf("ready> ");
}

static diag_span_T spans[DIAG_SPAN_N];
static diag_ta_T ta_stats;                  // all-zero state is an empty statistic     // all-zero state is an empty statistic (see diag_span_record)
unsigned int G_buf_max[DIAG_BUF_N];
unsigned int G_buf_eth_rows;

void DIAG_buf_clear(void) {
	for (int i = 0; i < DIAG_BUF_N; i++) {
		G_buf_max[i] = 0;
	}
	G_buf_eth_rows = 0;
	core_util_critical_section_enter();
	for (int i = 0; i < DIAG_SPAN_N; i++) {
		diag_span_clear(&spans[i]);
	}
	diag_ta_clear(&ta_stats);
	core_util_critical_section_exit();
}

// The lines of 'buffers' without the prompt. The sizes are those of the arrays in global_variables.cpp (TX FIFO internal
// 128 blocks of 128 bytes, external 1024 blocks, RX_FIFO_data 0x2000 bytes) and of ethernet_buffer in L1L2_radio.cpp
// (one row of 1600 bytes minus the 14 byte Ethernet header).
static void buffers_lines(void) {
	char line[100];
	HMI_printf("highest fill since the start or 'clear stats':\r\n");
	diag_stack_line("TX FIFO internal", G_buf_max[DIAG_BUF_TX_INT], 128, line, sizeof(line));
	HMI_printf("%s", line);
	if (is_SRAM_ext) {
		diag_stack_line("TX FIFO external", G_buf_max[DIAG_BUF_TX_EXT], 1024, line, sizeof(line));
		HMI_printf("%s", line);
	} else {
		HMI_printf("  TX FIFO external     no SRAM chip detected\r\n");
	}
	diag_stack_line("RX_FIFO_data", G_buf_max[DIAG_BUF_RX_FIFO], 0x2000, line, sizeof(line));
	HMI_printf("%s", line);
	diag_stack_line("ethernet_buffer row", G_buf_max[DIAG_BUF_ETH], 1586, line, sizeof(line));
	HMI_printf("%s", line);
	HMI_printf("  rows of ethernet_buffer used (bit mask): 0x%02X, SRAM chip: %s\r\n", G_buf_eth_rows, is_SRAM_ext ? "yes" : "no");
}

void DIAG_buffers_print(void) {
	buffers_lines();
	HMI_printf("ready> ");
}

void DIAG_ta_count(int what) {
	diag_ta_count(&ta_stats, what);
}

void DIAG_ta_value(int what, int a, int client) {
	diag_ta_value(&ta_stats, what, a, client);
}

// The lines of 'ta' without the prompt. The window is the one of TDMA_TA_measure_single_frame (-200 to 2000 us).
static void ta_lines(void) {
	if (!is_TDMA_master) {
		HMI_printf("not measured here, value from the master (WHOIS), own entry %ld (raw, 1/10 us)\r\n",
			(my_radio_client_ID < radio_addr_table_size) ? TDMA_table_TA[my_radio_client_ID] : 0L);
		return;
	}
	diag_ta_T s = ta_stats;
	HMI_printf("TA measurement at the master since the start or 'clear stats' (window -200 to 2000 us):\r\n");
	HMI_printf("  frames received          %6u\r\n", (unsigned int)s.n[DIAG_TA_FRAME]);
	HMI_printf("  uplink with synchro bit  %6u\r\n", (unsigned int)s.n[DIAG_TA_SYNC_UP]);
	HMI_printf("  wrong parity             %6u\r\n", (unsigned int)s.n[DIAG_TA_PARITY]);
	HMI_printf("  client not in the table  %6u\r\n", (unsigned int)s.n[DIAG_TA_UNKNOWN]);
	HMI_printf("  client not connected     %6u\r\n", (unsigned int)s.n[DIAG_TA_UNCONNECTED]);
	HMI_printf("  accepted                 %6u\r\n", (unsigned int)s.n[DIAG_TA_ACCEPTED]);
	HMI_printf("  below the window         %6u\r\n", (unsigned int)s.n[DIAG_TA_BELOW]);
	HMI_printf("  above the window         %6u\r\n", (unsigned int)s.n[DIAG_TA_ABOVE]);
	if (s.values) {
		HMI_printf("  raw TA last %d us, smallest %d us, largest %d us (%u values)\r\n", (int)s.last, (int)s.min, (int)s.max, (unsigned int)s.values);
	} else {
		HMI_printf("  no raw value yet\r\n");
	}
	for (int i = 0; i < radio_addr_table_size; i++) {
		if (CONF_radio_addr_table_status[i]) {
			if (G_ta_valid[i]) {
				HMI_printf("  client %d: TA filtered %ld us (display %.1f km), %u accepted since clear\r\n", i, TDMA_table_TA[i] / 10, 0.15 * (float)(TDMA_table_TA[i] / 10), (unsigned int)s.accepted_client[i]);
			} else {
				HMI_printf("  client %d: TA n/a (no measurement accepted yet), %u accepted since clear\r\n", i, (unsigned int)s.accepted_client[i]);
			}
		}
	}
}

void DIAG_ta_print(void) {
	ta_lines();
	HMI_printf("ready> ");
}

void DIAG_span_note(int which, unsigned int us) {
	if ( (which >= 0) && (which < DIAG_SPAN_N) && (us < 20000) ) {
		diag_span_record(&spans[which], us);
	}
}

// The lines of 'txlat' without the prompt, from a copy taken with interrupts off (the spans are written in an interrupt).
static void txlat_lines(void) {
	diag_span_T s[DIAG_SPAN_N];
	core_util_critical_section_enter();
	for (int i = 0; i < DIAG_SPAN_N; i++) {
		s[i] = spans[i];
	}
	core_util_critical_section_exit();
	char line[120];
	HMI_printf("start of a transmission, microseconds, since the start or 'clear stats':\r\n");
	diag_span_line("prepa_TX_1 to start order", &s[DIAG_SPAN_CHAIN], line, sizeof(line));
	HMI_printf("%s", line);
	HMI_printf("  designed value (CONF_delay_prepTX1_2_TX): %d us\r\n", CONF_delay_prepTX1_2_TX);
	if (is_TDMA_master) {
		diag_span_line("master top stamp to start order", &s[DIAG_SPAN_TOP], line, sizeof(line));
		HMI_printf("%s", line);
	}
}

void DIAG_txlat_print(void) {
	txlat_lines();
	HMI_printf("ready> ");
}

static Ticker lat_ticker;
static diag_lat_T lat;
static int lat_running = 0;
static uint32_t lat_start_us;
static uint32_t lat_n;

static void lat_callback(void) {
	uint32_t now = us_ticker_read();
	lat_n++;
	diag_lat_record(&lat, (int32_t)((now - lat_start_us) - lat_n * DIAG_LAT_PERIOD_US));
}

static void lat_start(void) {
	if (lat_running) {
		return;
	}
	lat_n = 0;
	lat_start_us = us_ticker_read();
	lat_running = 1;
	lat_ticker.attach(&lat_callback, std::chrono::microseconds(DIAG_LAT_PERIOD_US));
}

static void lat_stop(void) {
	if (lat_running) {
		lat_ticker.detach();
		lat_running = 0;
	}
}

// The lines of 'latency' without the prompt, from a copy taken with interrupts off.
static void latency_lines(void) {
	diag_lat_T s;
	core_util_critical_section_enter();
	s = lat;
	int running = lat_running;
	core_util_critical_section_exit();
	char line[100];
	for (int i = 0; diag_lat_line(&s, running, i, line, sizeof(line)); i++) {
		HMI_printf("%s", line);
	}
}

void DIAG_latency_command(const char* arg) {
	if ( (arg != NULL) && (strcmp(arg, "start") == 0) ) {
		diag_lat_clear(&lat);
		lat_start();
		HMI_printf("latency probe started (1 ms Ticker callback), read it with 'latency', stop with 'latency stop'\r\nready> ");
		return;
	}
	if ( (arg != NULL) && (strcmp(arg, "stop") == 0) ) {
		lat_stop();
		HMI_printf("latency probe stopped\r\nready> ");
		return;
	}
	if ( (arg != NULL) && (strcmp(arg, "clear") == 0) ) {
		core_util_critical_section_enter();
		diag_lat_clear(&lat);
		lat_n = 0;
		lat_start_us = us_ticker_read();
		core_util_critical_section_exit();
		HMI_printf("latency statistics cleared\r\nready> ");
		return;
	}
	if (arg != NULL) {
		HMI_printf("usage: latency [start|stop|clear]\r\nready> ");
		return;
	}
	latency_lines();
	HMI_printf("ready> ");
}

void DIAG_debug_lines(void) {
	HMI_printf("== stack ==\r\n");
	stack_lines();
	HMI_printf("== buffers ==\r\n");
	buffers_lines();
	HMI_printf("== tx timing ==\r\n");
	txlat_lines();
	HMI_printf("== ta ==\r\n");
	ta_lines();
	HMI_printf("== latency ==\r\n");
	if ( (lat.count == 0) && (!lat_running) ) {
		HMI_printf("probe not started ('latency start')\r\n");
	} else {
		latency_lines();
	}
}

#else // NPR_DIAG == 0

void DIAG_init(void) {}
void DIAG_stack_print(void) { HMI_printf("not built in (NPR_DIAG=0)\r\nready> "); }
void DIAG_buffers_print(void) { HMI_printf("not built in (NPR_DIAG=0)\r\nready> "); }
void DIAG_txlat_print(void) { HMI_printf("not built in (NPR_DIAG=0)\r\nready> "); }
void DIAG_ta_print(void) { HMI_printf("not built in (NPR_DIAG=0)\r\nready> "); }
void DIAG_ta_count(int) {}
void DIAG_ta_value(int, int, int) {}
void DIAG_span_note(int, unsigned int) {}
void DIAG_buf_clear(void) {}
void DIAG_latency_command(const char*) { HMI_printf("not built in (NPR_DIAG=0)\r\nready> "); }
void DIAG_debug_lines(void) {}

#endif
