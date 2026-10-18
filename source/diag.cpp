// Diagnosis commands of the console: 'stack' (stack and heap usage) and 'latency' (jitter of the 1 ms Ticker callback).
// The Ticker callback is the mechanism the TDMA timing uses to start a transmission (attach_us in SI4463.cpp and
// TDMA.cpp), so its lateness is the jitter of those start times. It includes every delay of the interrupt: other
// interrupts, sections with interrupts disabled. It is not the delay of the radio interrupt pin itself.
#include "mbed.h"
#include "hal/us_ticker_api.h"
#include "HMI_telnet.h"
#include "diag.h"
#include "diag_core.h"
#include <malloc.h>

#if NPR_DIAG

#if defined(MBED_MAJOR_VERSION) && (MBED_MAJOR_VERSION >= 6)
#include "platform/mbed_stats.h"
#define DIAG_MBED6 1
extern "C" {
	extern unsigned char* mbed_stack_isr_start;
	extern uint32_t mbed_stack_isr_size;
	extern uint32_t mbed_heap_size_total;
}
#endif

#define DIAG_LAT_PERIOD_US 1000

#ifdef DIAG_MBED6
static int isr_filled = 0;
#endif

void DIAG_init(void) {
#ifdef DIAG_MBED6
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
#endif
}

// The lines of 'stack' without the prompt.
static void stack_lines(void) {
#ifdef DIAG_MBED6
	char line[100];
#endif
	HMI_printf("stack use, highest values since the start (bytes):\r\n");
#if defined(MBED_THREAD_STATS_ENABLED) && defined(DIAG_MBED6)
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
#ifdef DIAG_MBED6
	if (isr_filled) {
		diag_stack_line("interrupt stack", diag_stack_used_bytes((const uint32_t*)mbed_stack_isr_start, mbed_stack_isr_size / 4), mbed_stack_isr_size, line, sizeof(line));
		HMI_printf("%s", line);
	} else {
		HMI_printf("  interrupt stack: not measured\r\n");
	}
#endif
	struct mallinfo mi = mallinfo();
#ifdef DIAG_MBED6
	HMI_printf("heap: %u bytes in use, %u taken so far of the heap area of %u bytes\r\n", (unsigned int)mi.uordblks, (unsigned int)mi.arena, (unsigned int)mbed_heap_size_total);
#else
	HMI_printf("heap: %u bytes in use, %u taken so far\r\n", (unsigned int)mi.uordblks, (unsigned int)mi.arena);
#endif
}

void DIAG_stack_print(void) {
	stack_lines();
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
#ifdef DIAG_MBED6
	lat_ticker.attach(&lat_callback, std::chrono::microseconds(DIAG_LAT_PERIOD_US));
#else
	lat_ticker.attach_us(&lat_callback, DIAG_LAT_PERIOD_US);
#endif
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
void DIAG_latency_command(const char*) { HMI_printf("not built in (NPR_DIAG=0)\r\nready> "); }
void DIAG_debug_lines(void) {}

#endif
