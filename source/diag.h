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

// The sections "== stack ==" and "== latency ==" of the 'debug' output (no prompt).
void DIAG_debug_lines(void);

#endif
