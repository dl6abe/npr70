// libFuzzer target for the W5500 receive functions: W5500_read_UDP_pckt and W5500_read_MAC_pckt (W5500.cpp, cut out by
// tools/fuzz.sh like in mem_host) against a fake chip. The chip hands out the bytes of the input as the content of its
// receive buffer, the destination buffer is a heap block of exactly the size the caller names, so ASan reports a write
// behind it.
// Byte 0: bit 0 = 1 MAC (MACRAW socket), 0 UDP. Bytes 1 and 2: buffer size (UDP at least 8, MAC at least 2, as the
// callers pass 592 or 1600). Bytes 3 and 4: read pointer register. The rest: content of the receive buffer.
#include "mbed.h"
#include "W5500.h"

static const uint8_t *chip_data;
static size_t chip_size, chip_pos;
static unsigned int rd_pointer;
static unsigned int cmd_addr;
int SPI::write(const char* tx, int tx_len, char* rx, int rx_len) {
    if (tx && tx_len == 3 && rx) {                        // register read: three command bytes, then the value
        unsigned int addr = ((unsigned char)tx[0] << 8) | (unsigned char)tx[1];
        memset(rx, 0, rx_len);
        if (addr == W5500_Sn_RX_RD0 && rx_len >= 5) { rx[3] = rd_pointer >> 8; rx[4] = rd_pointer & 0xFF; }
        return 0;
    }
    if (tx && tx_len == 3 && !rx) { cmd_addr = ((unsigned char)tx[0] << 8) | (unsigned char)tx[1]; return 0; }
    if (!tx && rx && rx_len > 0) {                        // bytes of the receive buffer
        for (int i = 0; i < rx_len; i++) rx[i] = chip_pos < chip_size ? (char)chip_data[chip_pos++] : 0;
        return 0;
    }
    return 0;
}
#include "W5500_part.inc"
#include "W5500_mac.inc"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size < 5) return 0;
    bool mac = data[0] & 1;
    unsigned int len = data[1] | (data[2] << 8);
    rd_pointer = data[3] | (data[4] << 8);
    chip_data = data + 5; chip_size = size - 5; chip_pos = 0;
    if (len > 2000) len = 2000;
    if (mac ? len < 2 : len < 8) return 0;
    W5500_chip chip;
    SPI_F4HDK spi; DigitalOut cs; DigitalIn irq;
    chip.spi_port = &spi; chip.cs = &cs; chip.interrupt = &irq;
    unsigned char *buf = (unsigned char *)malloc(len);
    memset(buf, 0, len);
    if (mac) {
        int n = W5500_read_MAC_pckt(&chip, 1, buf, (int)len);
        if (n != 0 && (n <= 2 || n > (int)len)) __builtin_trap();       // 0, or a frame that fits
    } else {
        unsigned int n = W5500_read_UDP_pckt(&chip, 1, buf, len);
        if (n < 8 || n > len) __builtin_trap();                          // never more than the buffer holds
    }
    free(buf);
    return 0;
}
