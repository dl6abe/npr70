// Diagnosis helpers without any dependency on the target: statistics of the Ticker latency probe and the evaluation of
// a stack that was filled with a pattern. diag.cpp uses them on the modem, tests/diag_host tests them on the host.
#ifndef DIAG_CORE_H
#define DIAG_CORE_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#define DIAG_LAT_BUCKETS 8
#define DIAG_STACK_PATTERN 0xA5A5A5A5UL

// Upper limits of the buckets in microseconds; the last bucket has no limit.
static const uint32_t DIAG_LAT_LIMIT[DIAG_LAT_BUCKETS - 1] = {5, 10, 20, 50, 100, 200, 500};

typedef struct {
	uint32_t count;               // callbacks seen
	uint64_t sum_us;              // sum of the lateness, for the mean
	uint32_t max_us;              // largest lateness
	int32_t base_us;              // the earliest callback seen so far (offset to the nominal grid), the zero of the lateness
	int started;
	uint32_t hist[DIAG_LAT_BUCKETS];
} diag_lat_T;

static inline void diag_lat_clear(diag_lat_T* s) {
	s->count = 0;
	s->sum_us = 0;
	s->max_us = 0;
	s->base_us = 0;
	s->started = 0;
	for (int i = 0; i < DIAG_LAT_BUCKETS; i++) {
		s->hist[i] = 0;
	}
}

static inline int diag_lat_bucket(uint32_t late_us) {
	for (int i = 0; i < DIAG_LAT_BUCKETS - 1; i++) {
		if (late_us <= DIAG_LAT_LIMIT[i]) {
			return i;
		}
	}
	return DIAG_LAT_BUCKETS - 1;
}

// offset_us = (time of the callback - start) - n * period: how far the callback is from the nominal grid. The earliest
// callback seen defines the best case (entry of the interrupt without any delay), the lateness is measured from there.
static inline void diag_lat_record(diag_lat_T* s, int32_t offset_us) {
	if ( (!s->started) || (offset_us < s->base_us) ) {
		s->base_us = offset_us;
		s->started = 1;
	}
	uint32_t late = (uint32_t)(offset_us - s->base_us);
	s->count++;
	s->sum_us += late;
	if (late > s->max_us) {
		s->max_us = late;
	}
	s->hist[diag_lat_bucket(late)]++;
}

// A stack of `words` 32 bit words was filled with DIAG_STACK_PATTERN from its lowest address (the end of the stack)
// up to some point. It grows downwards from the top. The number of bytes used is the size minus the unchanged words
// at the low end.
static inline uint32_t diag_stack_used_bytes(const uint32_t* low, size_t words) {
	size_t free_words = 0;
	while ( (free_words < words) && (low[free_words] == DIAG_STACK_PATTERN) ) {
		free_words++;
	}
	return (uint32_t)((words - free_words) * 4);
}

// Text lines of the 'latency' output (the console buffer HMI_out_str has 120 bytes, so every line is limited by
// snprintf). Line 0 is the state, line 1 the mean and maximum (or "no data"), lines 2 and 3 the histogram. Returns 0
// when there is no line with this index.
static inline int diag_lat_line(const diag_lat_T* s, int running, int index, char* out, size_t size) {
	switch (index) {
	case 0:
		snprintf(out, size, "latency probe: %s, %lu callbacks of the 1 ms Ticker\r\n", running ? "running" : "stopped", (unsigned long)s->count);
		return 1;
	case 1:
		if (s->count == 0) {
			snprintf(out, size, "no data, start it with 'latency start'\r\n");
		} else {
			snprintf(out, size, "lateness against the earliest callback: mean %lu us, maximum %lu us\r\n", (unsigned long)(s->sum_us / s->count), (unsigned long)s->max_us);
		}
		return 1;
	case 2:
		if (s->count == 0) {
			return 0;
		}
		snprintf(out, size, "  <=5us %lu  <=10us %lu  <=20us %lu  <=50us %lu\r\n", (unsigned long)s->hist[0], (unsigned long)s->hist[1], (unsigned long)s->hist[2], (unsigned long)s->hist[3]);
		return 1;
	case 3:
		if (s->count == 0) {
			return 0;
		}
		snprintf(out, size, "  <=100us %lu  <=200us %lu  <=500us %lu  >500us %lu\r\n", (unsigned long)s->hist[4], (unsigned long)s->hist[5], (unsigned long)s->hist[6], (unsigned long)s->hist[7]);
		return 1;
	}
	return 0;
}

// Statistics of the TA measurement at the master (issue #47). TDMA_TA_measure_single_frame drops a measurement outside of
// the window and a frame without the synchro bit or with a wrong parity without a trace; these counters show which.
#define DIAG_TA_CLIENTS 7
enum { DIAG_TA_FRAME, DIAG_TA_SYNC_UP, DIAG_TA_PARITY, DIAG_TA_UNKNOWN, DIAG_TA_UNCONNECTED, DIAG_TA_ACCEPTED, DIAG_TA_BELOW, DIAG_TA_ABOVE, DIAG_TA_N };

typedef struct {
	uint32_t n[DIAG_TA_N];
	uint32_t accepted_client[DIAG_TA_CLIENTS];
	uint32_t values;                 // raw values recorded (accepted, below, above)
	int32_t last, min, max;          // raw TA_answer in microseconds
} diag_ta_T;

static inline void diag_ta_clear(diag_ta_T* s) {
	memset(s, 0, sizeof(*s));
}

static inline void diag_ta_count(diag_ta_T* s, int what) {
	if ( (what >= 0) && (what < DIAG_TA_N) ) s->n[what]++;
}

// A raw measurement of a known client: counts accepted / below / above and keeps last, smallest and largest value.
static inline void diag_ta_value(diag_ta_T* s, int what, int32_t a, int client) {
	diag_ta_count(s, what);
	if ( (what == DIAG_TA_ACCEPTED) && (client >= 0) && (client < DIAG_TA_CLIENTS) ) s->accepted_client[client]++;
	if (s->values == 0 || a < s->min) s->min = a;
	if (s->values == 0 || a > s->max) s->max = a;
	s->last = a;
	s->values++;
}

// Fill of a FIFO from its two counters that only count up: write minus read, 0 when the read counter is ahead.
static inline int diag_buf_fill(unsigned int wr, unsigned int rd) {
	int d = (int)(wr - rd);
	return d < 0 ? 0 : d;
}

// Statistics of a time span in microseconds (issue #41): how long the start of a transmission takes.
typedef struct {
	uint32_t count;
	uint32_t min_us;
	uint32_t max_us;
	uint32_t last_us;
	uint64_t sum_us;
} diag_span_T;

static inline void diag_span_clear(diag_span_T* s) {
	s->count = 0;
	s->min_us = 0xFFFFFFFFu;
	s->max_us = 0;
	s->last_us = 0;
	s->sum_us = 0;
}

static inline void diag_span_record(diag_span_T* s, uint32_t us) {
	if ( (s->count == 0) || (us < s->min_us) ) s->min_us = us;   // an all-zero state is empty too
	if (us > s->max_us) s->max_us = us;
	s->count++;
	s->last_us = us;
	s->sum_us += us;
}

// One line of 'txlat': label, number of spans, smallest, mean, largest, last. Without a span only the label.
static inline void diag_span_line(const char* label, const diag_span_T* s, char* out, size_t size) {
	if (s->count == 0) {
		snprintf(out, size, "  %-34s none yet\r\n", label);
		return;
	}
	snprintf(out, size, "  %-34s n %6lu  min %5lu  mean %5lu  max %5lu  last %5lu\r\n", label, (unsigned long)s->count, (unsigned long)s->min_us,
	         (unsigned long)(s->sum_us / s->count), (unsigned long)s->max_us, (unsigned long)s->last_us);
}

// One line of the 'stack' output: label, bytes used, size, percent.
static inline void diag_stack_line(const char* label, unsigned int used, unsigned int total, char* out, size_t size) {
	snprintf(out, size, "  %-20s %5u of %5u (%u %%)\r\n", label, used, total, total ? (unsigned int)(100ULL * used / total) : 0u);
}

#endif
