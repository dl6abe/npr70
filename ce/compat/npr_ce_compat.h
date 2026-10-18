// Compatibility layer for Mbed OS CE. Force-included into every firmware source file (npr70-ce/CMakeLists.txt),
// so the sources in npr70/source stay unchanged for mbed-os 5.15.9.
//
// mbed::Serial is gone in Mbed OS 6 and CE. The firmware uses one Serial object (pc) for the console:
// readable(), getc(pc), putc(c, pc), printf() and baud(). This class maps those calls to the console
// FileHandle of the platform (a BufferedSerial, set by platform.stdio-* in mbed_app.json). The baud rate and
// the pins come from the configuration, so baud() and the constructor arguments are ignored.
#pragma once
#include "mbed.h"
#include <cstdarg>
#include <cstdio>

// All device interrupts start with priority 0. An interrupt cannot preempt another one of the same priority, so the
// UART receive interrupt waits for a long Timeout or Ticker callback of the firmware (the callbacks do SPI transfers to
// the radio chip). At 921600 baud a character arrives every 10.9 us and a wait longer than that loses characters.
// This puts the console UART above every other enabled device interrupt (priority 0 against 1). The other interrupts
// keep the same priority among each other, so they still do not preempt each other as before. Called from
// readable(): it runs every pass of the main loop, so interrupts that the firmware enables later are caught too.
static inline void npr_uart_irq_above_others()
{
    for (int irq = 0; irq <= (int)FPU_IRQn; irq++) {
        if (irq != (int)USART2_IRQn && NVIC_GetEnableIRQ((IRQn_Type)irq)) {
            NVIC_SetPriority((IRQn_Type)irq, 1);
        }
    }
    NVIC_SetPriority(USART2_IRQn, 0);
}

class Serial {
    static FileHandle *console() { return mbed::mbed_file_handle(STDIN_FILENO); }
public:
    Serial(PinName, PinName) {}
    void baud(int) {}
    bool readable() {
#ifdef NPR_UART_IRQ_PRIORITY
        static unsigned calls;
        if ((calls++ & 0x3FF) == 0) {
            npr_uart_irq_above_others();
        }
#endif
        return (console()->poll(POLLIN) & POLLIN) != 0;
    }
    int getc_() {
        char c;
        return console()->read(&c, 1) == 1 ? (unsigned char)c : -1;
    }
    int putc_(int c) {
#ifdef NPR_DBG_NO_ECHO
        return c;   // measurement only: no echo, to see whether echo writes disturb the receive path
#endif
        char ch = (char)c;
        console()->write(&ch, 1);
        return c;
    }
    int printf(const char *fmt, ...) {
        char buf[256];
        va_list ap;
        va_start(ap, fmt);
        int n = vsnprintf(buf, sizeof buf, fmt, ap);
        va_end(ap);
        if (n > (int)sizeof buf - 1) {
            n = sizeof buf - 1;
        }
        if (n > 0) {
            console()->write(buf, n);
        }
        return n;
    }
};

static inline int getc(Serial &p) { return p.getc_(); }
static inline int putc(int c, Serial &p) { return p.putc_(c); }
