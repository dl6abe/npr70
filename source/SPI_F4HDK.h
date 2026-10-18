#ifndef __SPI_F4HDK_H__
#define __SPI_F4HDK_H__

#include "mbed.h"
#include "SPI.h"

/**
 * SPI with one extra method, `transfer_2` (F4HDK). The original code added it
 * to the MbedOS files `SPI.h`, `SPI.cpp`, `spi_api.h` and `stm_spi_api.c`.
 * Inheritance keeps MbedOS unmodified (approach of slintak/NPR70).
 */
class SPI_F4HDK: public SPI {
public:
    using SPI::SPI;

    // One HAL_SPI_TransmitReceive over max(tx_length, rx_length) bytes.
    // No mutex and no _acquire(), as in the original patch.
    int transfer_2(const void *tx_buffer, int tx_length, void *rx_buffer, int rx_length);
};

#endif
