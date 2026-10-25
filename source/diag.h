// Diagnosis commands of the console: 'stack' (stack and heap usage) and 'latency' (jitter of the 1 ms Ticker callback).
// NPR_DIAG=0 at the build leaves them out (the commands then say so).
#ifndef DIAG_H
#define DIAG_H

#ifndef NPR_DIAG
#define NPR_DIAG 1
#endif

// Fill the unused part of the interrupt stack with a pattern. Call first in main().
void DIAG_init(void);

// 'stack': prints the stack use of every thread, of the interrupt stack and the heap, then the prompt.
void DIAG_stack_print(void);

// 'latency [start|stop|clear]': prints the statistics, or starts, stops, clears the probe, then the prompt.
void DIAG_latency_command(const char* arg);

// 'buffers': the highest fill of the big buffers since the start or since 'clear stats' (issue #37), then the prompt.
void DIAG_buffers_print(void);

// Sets the highest fill values back to 0 ('clear stats').
void DIAG_buf_clear(void);

// 'txlat': how long the start of a transmission takes (issue #41), then the prompt. The spans are measured in the
// interrupt context of SI4463_TX_new_frame: DIAG_SPAN_CHAIN from the call of SI4463_prepa_TX_1 to the end of the start
// order of the first frame of a burst (CONF_delay_prepTX1_2_TX is the designed value), DIAG_SPAN_TOP from the stamp
// of the master top (TDMA_top_measure) to the same point. The master top span goes into the TA display.
enum { DIAG_SPAN_CHAIN, DIAG_SPAN_TOP, DIAG_SPAN_N };
void DIAG_txlat_print(void);
void DIAG_span_note(int which, unsigned int us);   // ignores values above 20 ms (no valid span)

// The sections "== stack ==", "== buffers ==" and "== latency ==" of the 'debug' output (no prompt).
void DIAG_debug_lines(void);

// 'ta': statistics of the TA measurement at the master (issue #47), then the prompt. The counters are written in
// TDMA_TA_measure_single_frame (thread level, called from radio_RX_FIFO_dequeue). 'clear stats' resets them.
void DIAG_ta_print(void);
void DIAG_ta_count(int what);                                   // what: DIAG_TA_*, see diag_core.h
void DIAG_ta_value(int what, int a, int client);

// Highest fill of a buffer. Called from the thread level only (a plain compare and store, no interrupt calls it).
enum { DIAG_BUF_TX_INT, DIAG_BUF_TX_EXT, DIAG_BUF_RX_FIFO, DIAG_BUF_ETH, DIAG_BUF_N };
#if NPR_DIAG
extern unsigned int G_buf_max[DIAG_BUF_N];
extern unsigned int G_buf_eth_rows;     // bit mask of the rows of ethernet_buffer that were used
static inline void DIAG_buf_note(int which, int fill) {
	if ( (fill > 0) && ((unsigned int)fill > G_buf_max[which]) ) {
		G_buf_max[which] = (unsigned int)fill;
	}
}
static inline void DIAG_buf_eth_row(unsigned int row) { G_buf_eth_rows |= 1u << (row & 31); }
#else
static inline void DIAG_buf_note(int, int) {}
static inline void DIAG_buf_eth_row(unsigned int) {}
#endif

#endif
