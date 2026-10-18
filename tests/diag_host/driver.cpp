// Cases for diag_core.h: lateness statistics of the latency probe and the evaluation of a pattern filled stack.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "diag_core.h"

static int failures = 0;
#define CHECK(cond, ...) do { if (!(cond)) { failures++; printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

int main() {
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
