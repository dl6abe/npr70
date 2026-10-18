#include "spi_api.h"
#include "SPI_F4HDK.h"

#if DEVICE_SPI_ASYNCH
    #define SPI_S(obj)    (( struct spi_s *)(&(obj->spi)))
#else
    #define SPI_S(obj)    (( struct spi_s *)(obj))
#endif

int SPI_F4HDK::transfer_2(const void *tx_buffer, int tx_length, void *rx_buffer, int rx_length) {
    // mbed-os 5.12 and later keep the SPI state in _peripheral->spi, earlier versions in _spi.
    // build_port.sh sets NPR_SPI_PERIPHERAL for the new layout; without it the old name is used.
#if defined(NPR_SPI_PERIPHERAL) && NPR_SPI_PERIPHERAL
    struct spi_s *spiobj = SPI_S((&_peripheral->spi));
#else
    struct spi_s *spiobj = SPI_S((&_spi));
#endif
    SPI_HandleTypeDef *handle = &(spiobj->handle);

    if (tx_length < rx_length) {
        tx_length = rx_length;
    }

    /* 3 ms timeout */
    return HAL_SPI_TransmitReceive(handle, (uint8_t*)tx_buffer, (uint8_t*)rx_buffer, tx_length, 3);
}
