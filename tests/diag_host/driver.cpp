// Cases for diag_core.h: lateness statistics of the latency probe and the evaluation of a pattern filled stack.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "diag_core.h"

static int failures = 0;
#define CHECK(cond, ...) do { if (!(cond)) { failures++; printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

int main() {
	// ---- TA statistics of the master (issue #47)
	diag_ta_T ta;
	memset(&ta, 0xAA, sizeof(ta));
	diag_ta_clear(&ta);
	CHECK(ta.n[DIAG_TA_FRAME] == 0 && ta.values == 0 && ta.accepted_client[3] == 0, "cleared");
	diag_ta_count(&ta, DIAG_TA_FRAME);
	diag_ta_count(&ta, DIAG_TA_FRAME);
	diag_ta_count(&ta, DIAG_TA_SYNC_UP);
	diag_ta_count(&ta, DIAG_TA_N);                    // outside of the range: ignored
	diag_ta_count(&ta, -1);
	CHECK(ta.n[DIAG_TA_FRAME] == 2 && ta.n[DIAG_TA_SYNC_UP] == 1 && ta.n[DIAG_TA_PARITY] == 0, "counts");
	diag_ta_value(&ta, DIAG_TA_ACCEPTED, 154, 0);
	diag_ta_value(&ta, DIAG_TA_ACCEPTED, 150, 0);
	diag_ta_value(&ta, DIAG_TA_ABOVE, 2500, 0);
	diag_ta_value(&ta, DIAG_TA_BELOW, -400, 6);
	CHECK(ta.n[DIAG_TA_ACCEPTED] == 2 && ta.n[DIAG_TA_ABOVE] == 1 && ta.n[DIAG_TA_BELOW] == 1, "window classes");
	CHECK(ta.accepted_client[0] == 2 && ta.accepted_client[6] == 0, "accepted per client: only accepted values count");
	CHECK(ta.values == 4 && ta.last == -400 && ta.min == -400 && ta.max == 2500, "raw values %u last %d min %d max %d", (unsigned)ta.values, (int)ta.last, (int)ta.min, (int)ta.max);
	diag_ta_value(&ta, DIAG_TA_ACCEPTED, 5, 9);        // a client number outside of the table is not indexed
	CHECK(ta.n[DIAG_TA_ACCEPTED] == 3, "client outside of the table is counted but not indexed");
	// ---- spans of the start of a transmission (issue #41)
	diag_span_T sp;
	memset(&sp, 0, sizeof(sp));                    // the all-zero state of a static variable is empty
	char sline[160];
	diag_span_line("empty", &sp, sline, sizeof(sline));
	CHECK(strstr(sline, "none yet") != NULL, "empty span line: %s", sline);
	diag_span_record(&sp, 1100);
	CHECK(sp.count == 1 && sp.min_us == 1100 && sp.max_us == 1100 && sp.last_us == 1100, "first span %u %u %u", (unsigned)sp.count, (unsigned)sp.min_us, (unsigned)sp.max_us);
	diag_span_record(&sp, 1300);
	diag_span_record(&sp, 1000);
	CHECK(sp.count == 3 && sp.min_us == 1000 && sp.max_us == 1300 && sp.last_us == 1000 && sp.sum_us == 3400, "three spans");
	diag_span_line("chain", &sp, sline, sizeof(sline));
	CHECK(strstr(sline, "n      3") != NULL && strstr(sline, "min  1000") != NULL && strstr(sline, "mean  1133") != NULL && strstr(sline, "max  1300") != NULL, "line: %s", sline);
	diag_span_clear(&sp);
	CHECK(sp.count == 0 && sp.min_us == 0xFFFFFFFFu && sp.max_us == 0, "cleared");
	diag_span_record(&sp, 7);
	CHECK(sp.min_us == 7 && sp.max_us == 7, "after clear the first value is the smallest");
	// ---- FIFO fill from two counters that only count up (issue #37)
	CHECK(diag_buf_fill(10, 4) == 6, "fill 10-4");
	CHECK(diag_buf_fill(5, 5) == 0, "fill equal");
	CHECK(diag_buf_fill(3, 8) == 0, "read counter ahead gives 0, not a huge value");
	CHECK(diag_buf_fill(2, 0xFFFFFFFEu) == 4, "write counter wrapped: %d", diag_buf_fill(2, 0xFFFFFFFEu));
	CHECK(diag_buf_fill(200, 0) == 200, "a fill above the capacity of 128 stays visible (overrun)");
	// ---- buckets: <=5 <=10 <=20 <=50 <=100 <=200 <=500 >500
	struct { uint32_t us; int bucket; } b[] = {{0, 0}, {5, 0}, {6, 1}, {10, 1}, {11, 2}, {20, 2}, {21, 3}, {50, 3}, {51, 4}, {100, 4}, {101, 5},
		{200, 5}, {201, 6}, {500, 6}, {501, 7}, {100000, 7}, {0xFFFFFFFFu, 7}};
	for (unsigned int i = 0; i < sizeof(b)/sizeof(b[0]); i++)
		CHECK(diag_lat_bucket(b[i].us) == b[i].bucket, "bucket of %u us is %d, expected %d", (unsigned)b[i].us, diag_lat_bucket(b[i].us), b[i].bucket);
	// ---- statistics: the earliest callback is the zero of the lateness
	diag_lat_T s;
	diag_lat_clear(&s);
	CHECK(s.count == 0 && s.max_us == 0 && !s.started, "cleared state");
	diag_lat_record(&s, 12);      // first: base 12, lateness 0
	diag_lat_record(&s, 15);      // 3
	diag_lat_record(&s, 10);      // new earliest: base 10, lateness 0
	diag_lat_record(&s, 60);      // 50
	diag_lat_record(&s, 700);     // 690
	CHECK(s.count == 5 && s.base_us == 10, "count %u base %d", (unsigned)s.count, (int)s.base_us);
	CHECK(s.max_us == 690, "maximum %u", (unsigned)s.max_us);
	CHECK(s.sum_us == 0 + 3 + 0 + 50 + 690, "sum %llu", (unsigned long long)s.sum_us);
	CHECK(s.hist[0] == 3 && s.hist[3] == 1 && s.hist[7] == 1 && s.hist[1] == 0, "histogram %u %u %u %u", (unsigned)s.hist[0], (unsigned)s.hist[1], (unsigned)s.hist[3], (unsigned)s.hist[7]);
	uint32_t total = 0;
	for (int i = 0; i < DIAG_LAT_BUCKETS; i++) total += s.hist[i];
	CHECK(total == s.count, "histogram sums to %u, not %u", (unsigned)total, (unsigned)s.count);
	diag_lat_clear(&s);
	CHECK(s.count == 0 && s.sum_us == 0 && !s.started && s.hist[7] == 0, "cleared again");
	// a very long run: the 64 bit sum does not overflow (1000 callbacks per second with 1 ms lateness for 100 days)
	for (int i = 0; i < 1000; i++) diag_lat_record(&s, 1000);
	diag_lat_record(&s, 3000000000L > 2147483647L ? 2000000000 : 0);
	CHECK(s.max_us == 2000000000u - 1000u, "large offset: max %u", (unsigned)s.max_us);
	// negative offsets (a callback before the nominal grid) are possible
	diag_lat_clear(&s);
	diag_lat_record(&s, -5); diag_lat_record(&s, -8); diag_lat_record(&s, 20);
	CHECK(s.base_us == -8 && s.max_us == 28, "negative offsets: base %d max %u", (int)s.base_us, (unsigned)s.max_us);
	// ---- stack evaluation
	const size_t words = 256;                         // 1024 bytes like the interrupt stack
	uint32_t* st = (uint32_t*)malloc(words * 4);      // exact size for ASan
	for (size_t i = 0; i < words; i++) st[i] = DIAG_STACK_PATTERN;
	CHECK(diag_stack_used_bytes(st, words) == 0, "untouched stack");
	st[words - 1] = 0; st[words - 2] = 1;             // the top two words used
	CHECK(diag_stack_used_bytes(st, words) == 8, "two words used: %u", (unsigned)diag_stack_used_bytes(st, words));
	st[100] = 7;                                       // a word in the middle overwritten: everything above counts as used
	CHECK(diag_stack_used_bytes(st, words) == (words - 100) * 4, "used down to word 100: %u", (unsigned)diag_stack_used_bytes(st, words));
	st[0] = 3;                                         // the lowest word used: overflow of the stack
	CHECK(diag_stack_used_bytes(st, words) == words * 4, "whole stack used: %u", (unsigned)diag_stack_used_bytes(st, words));
	CHECK(diag_stack_used_bytes(st, 0) == 0, "empty stack");
	free(st);
	// ---- text lines of 'latency' and 'stack': content, and none longer than the 120 byte console buffer
	diag_lat_T t;
	diag_lat_clear(&t);
	char line[100];
	CHECK(diag_lat_line(&t, 0, 0, line, sizeof(line)) && strcmp(line, "latency probe: stopped, 0 callbacks of the 1 ms Ticker\r\n") == 0, "line 0 empty: %s", line);
	CHECK(diag_lat_line(&t, 0, 1, line, sizeof(line)) && strstr(line, "no data") != NULL, "line 1 empty: %s", line);
	CHECK(!diag_lat_line(&t, 0, 2, line, sizeof(line)) && !diag_lat_line(&t, 0, 3, line, sizeof(line)) && !diag_lat_line(&t, 0, 4, line, sizeof(line)), "no histogram lines without data");
	diag_lat_record(&t, 0); diag_lat_record(&t, 30); diag_lat_record(&t, 600);
	CHECK(diag_lat_line(&t, 1, 0, line, sizeof(line)) && strcmp(line, "latency probe: running, 3 callbacks of the 1 ms Ticker\r\n") == 0, "line 0: %s", line);
	CHECK(diag_lat_line(&t, 1, 1, line, sizeof(line)) && strcmp(line, "lateness against the earliest callback: mean 210 us, maximum 600 us\r\n") == 0, "line 1: %s", line);
	CHECK(diag_lat_line(&t, 1, 2, line, sizeof(line)) && strcmp(line, "  <=5us 1  <=10us 0  <=20us 0  <=50us 1\r\n") == 0, "line 2: %s", line);
	CHECK(diag_lat_line(&t, 1, 3, line, sizeof(line)) && strcmp(line, "  <=100us 0  <=200us 0  <=500us 0  >500us 1\r\n") == 0, "line 3: %s", line);
	// extreme values: 32 bit counters at their maximum, still inside 110 characters
	t.count = 0xFFFFFFFFu; t.sum_us = (uint64_t)0xFFFFFFFFu * 1000; t.max_us = 0xFFFFFFFFu;
	for (int i = 0; i < DIAG_LAT_BUCKETS; i++) t.hist[i] = 0xFFFFFFFFu;
	for (int i = 0; i < 4; i++) {
		diag_lat_line(&t, 1, i, line, sizeof(line));
		CHECK(strlen(line) <= 110, "line %d has %d characters", i, (int)strlen(line));
	}
	diag_stack_line("thread main", 2456, 4096, line, sizeof(line));
	CHECK(strcmp(line, "  thread main           2456 of  4096 (59 %)\r\n") == 0, "stack line: %s", line);
	diag_stack_line("interrupt stack", 528, 1024, line, sizeof(line));
	CHECK(strstr(line, "528 of  1024 (51 %)") != NULL, "stack line: %s", line);
	diag_stack_line("thread with a very long name that does not fit anywhere in the line", 4294967295u, 4294967295u, line, sizeof(line));
	CHECK(strlen(line) < sizeof(line), "long label is cut by snprintf");
	diag_stack_line("empty", 5, 0, line, sizeof(line));
	CHECK(strstr(line, "(0 %)") != NULL, "total 0: %s", line);
	printf("%s\n", failures ? "failed" : "all cases passed");
	return failures ? 1 : 0;
}
