/*
 * mbed SDK
 * Copyright (c) 2017 ARM Limited
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// Automatically generated configuration file.
// DO NOT EDIT, content will be overwritten.

#ifndef __MBED_CONFIG_DATA__
#define __MBED_CONFIG_DATA__

// Configuration parameters
#define MBED_CONF_EVENTS_SHARED_STACKSIZE                 1024        // set by library:events
#define MBED_CONF_DRIVERS_UART_SERIAL_RXBUF_SIZE          256         // set by library:drivers
#define MBED_CONF_DRIVERS_UART_SERIAL_TXBUF_SIZE          256         // set by library:drivers
#define MBED_CONF_PLATFORM_DEFAULT_SERIAL_BAUD_RATE       9600        // set by library:platform
#define MBED_CONF_EVENTS_SHARED_DISPATCH_FROM_APPLICATION 0           // set by library:events
#define MBED_CONF_PLATFORM_STDIO_CONVERT_NEWLINES         0           // set by library:platform
#define MBED_CONF_EVENTS_PRESENT                          1           // set by library:events
#define MBED_CONF_RTOS_PRESENT                            1           // set by library:rtos
#define MBED_CONF_EVENTS_SHARED_EVENTSIZE                 256         // set by library:events
#define MBED_CONF_PLATFORM_STDIO_BAUD_RATE                9600        // set by library:platform
#define CLOCK_SOURCE                                      USE_PLL_MSI // set by target:NUCLEO_L432KC
#define MBED_CONF_EVENTS_SHARED_HIGHPRIO_STACKSIZE        1024        // set by library:events
#define MBED_CONF_PLATFORM_STDIO_FLUSH_AT_EXIT            1           // set by library:platform
#define MBED_CONF_EVENTS_SHARED_HIGHPRIO_EVENTSIZE        256         // set by library:events

#define MBED_CONF_EVENTS_USE_LOWPOWER_TIMER_TICKER        0           // from export
#define MBED_CONF_PLATFORM_STDIO_BUFFERED_SERIAL          0           // from export
#define MBED_CONF_PLATFORM_STDIO_CONVERT_TTY_NEWLINES     0           // from export
#define MBED_CONF_TARGET_LOWPOWERTIMER_LPTIM              1           // from export
#define LPTICKER_DELAY_TICKS                              0 // from export
#define MBEDTLS_CIPHER_MODE_CTR                           // defined by library:SecureStore // from export
#define MBEDTLS_CMAC_C                                    // defined by library:SecureStore // from export
#define MBED_CONF_ATMEL_RF_ASSUME_SPACED_SPI              1 // from export
#define MBED_CONF_ATMEL_RF_FULL_SPI_SPEED                 7500000 // from export
#define MBED_CONF_ATMEL_RF_FULL_SPI_SPEED_BYTE_SPACING    250 // from export
#define MBED_CONF_ATMEL_RF_IRQ_THREAD_STACK_SIZE          1024 // from export
#define MBED_CONF_ATMEL_RF_LOW_SPI_SPEED                  3750000 // from export
#define MBED_CONF_ATMEL_RF_PROVIDE_DEFAULT                0 // from export
#define MBED_CONF_ATMEL_RF_USE_SPI_SPACING_API            0 // from export
#define MBED_CONF_DRIVERS_QSPI_CSN                        QSPI_FLASH1_CSN // from export
#define MBED_CONF_DRIVERS_QSPI_IO0                        QSPI_FLASH1_IO0 // from export
#define MBED_CONF_DRIVERS_QSPI_IO1                        QSPI_FLASH1_IO1 // from export
#define MBED_CONF_DRIVERS_QSPI_IO2                        QSPI_FLASH1_IO2 // from export
#define MBED_CONF_DRIVERS_QSPI_IO3                        QSPI_FLASH1_IO3 // from export
#define MBED_CONF_DRIVERS_QSPI_SCK                        QSPI_FLASH1_SCK // from export
#define MBED_CONF_FAT_CHAN_FFS_DBG                        0 // from export
#define MBED_CONF_FAT_CHAN_FF_CODE_PAGE                   437 // from export
#define MBED_CONF_FAT_CHAN_FF_FS_EXFAT                    0 // from export
#define MBED_CONF_FAT_CHAN_FF_FS_HEAPBUF                  1 // from export
#define MBED_CONF_FAT_CHAN_FF_FS_LOCK                     0 // from export
#define MBED_CONF_FAT_CHAN_FF_FS_MINIMIZE                 0 // from export
#define MBED_CONF_FAT_CHAN_FF_FS_NOFSINFO                 0 // from export
#define MBED_CONF_FAT_CHAN_FF_FS_NORTC                    0 // from export
#define MBED_CONF_FAT_CHAN_FF_FS_READONLY                 0 // from export
#define MBED_CONF_FAT_CHAN_FF_FS_REENTRANT                0 // from export
#define MBED_CONF_FAT_CHAN_FF_FS_RPATH                    1 // from export
#define MBED_CONF_FAT_CHAN_FF_FS_TIMEOUT                  1000 // from export
#define MBED_CONF_FAT_CHAN_FF_FS_TINY                     1 // from export
#define MBED_CONF_FAT_CHAN_FF_LFN_BUF                     255 // from export
#define MBED_CONF_FAT_CHAN_FF_LFN_UNICODE                 0 // from export
#define MBED_CONF_FAT_CHAN_FF_MAX_LFN                     255 // from export
#define MBED_CONF_FAT_CHAN_FF_MAX_SS                      4096 // from export
#define MBED_CONF_FAT_CHAN_FF_MIN_SS                      512 // from export
#define MBED_CONF_FAT_CHAN_FF_MULTI_PARTITION             0 // from export
#define MBED_CONF_FAT_CHAN_FF_NORTC_MDAY                  1 // from export
#define MBED_CONF_FAT_CHAN_FF_NORTC_MON                   1 // from export
#define MBED_CONF_FAT_CHAN_FF_NORTC_YEAR                  2017 // from export
#define MBED_CONF_FAT_CHAN_FF_SFN_BUF                     12 // from export
#define MBED_CONF_FAT_CHAN_FF_STRF_ENCODE                 3 // from export
#define MBED_CONF_FAT_CHAN_FF_STR_VOLUME_ID               0 // from export
#define MBED_CONF_FAT_CHAN_FF_SYNC_T                      HANDLE // from export
#define MBED_CONF_FAT_CHAN_FF_USE_CHMOD                   0 // from export
#define MBED_CONF_FAT_CHAN_FF_USE_EXPAND                  0 // from export
#define MBED_CONF_FAT_CHAN_FF_USE_FASTSEEK                0 // from export
#define MBED_CONF_FAT_CHAN_FF_USE_FIND                    0 // from export
#define MBED_CONF_FAT_CHAN_FF_USE_FORWARD                 0 // from export
#define MBED_CONF_FAT_CHAN_FF_USE_LABEL                   0 // from export
#define MBED_CONF_FAT_CHAN_FF_USE_LFN                     3 // from export
#define MBED_CONF_FAT_CHAN_FF_USE_MKFS                    1 // from export
#define MBED_CONF_FAT_CHAN_FF_USE_STRFUNC                 0 // from export
#define MBED_CONF_FAT_CHAN_FF_USE_TRIM                    1 // from export
#define MBED_CONF_FAT_CHAN_FF_VOLUMES                     4 // from export
#define MBED_CONF_FAT_CHAN_FF_VOLUME_STRS                 "RAM","NAND","CF","SD","SD2","USB","USB2","USB3" // from export
#define MBED_CONF_FAT_CHAN_FLUSH_ON_NEW_CLUSTER           0 // from export
#define MBED_CONF_FAT_CHAN_FLUSH_ON_NEW_SECTOR            1 // from export
#define MBED_CONF_GEMALTO_CINTERION_BAUDRATE              115200 // from export
#define MBED_CONF_GEMALTO_CINTERION_PROVIDE_DEFAULT       0 // from export
#define MBED_CONF_GENERIC_AT3GPP_BAUDRATE                 115200 // from export
#define MBED_CONF_GENERIC_AT3GPP_PROVIDE_DEFAULT          0 // from export
#define MBED_CONF_MCR20A_PROVIDE_DEFAULT                  0 // from export
#define MBED_CONF_PLATFORM_CTHUNK_COUNT_MAX               8 // from export
#define MBED_CONF_PLATFORM_ERROR_ALL_THREADS_INFO         0 // from export
#define MBED_CONF_PLATFORM_ERROR_HIST_SIZE                4 // from export
#define MBED_CONF_PLATFORM_ERROR_REBOOT_MAX               1 // from export
#define MBED_CONF_PLATFORM_MINIMAL_PRINTF_SET_FLOATING_POINT_MAX_DECIMALS 6 // from export
#define MBED_CONF_PLATFORM_POLL_USE_LOWPOWER_TIMER        0 // from export
#define MBED_CONF_PLATFORM_STDIO_MINIMAL_CONSOLE_ONLY     0 // from export
#define MBED_CONF_PLATFORM_USE_MPU                        1 // from export
#define MBED_CONF_QUECTEL_BC95_BAUDRATE                   9600 // from export
#define MBED_CONF_QUECTEL_BC95_PROVIDE_DEFAULT            0 // from export
#define MBED_CONF_QUECTEL_BG96_BAUDRATE                   115200 // from export
#define MBED_CONF_QUECTEL_BG96_PROVIDE_DEFAULT            0 // from export
#define MBED_CONF_QUECTEL_EC2X_BAUDRATE                   115200 // from export
#define MBED_CONF_QUECTEL_EC2X_PROVIDE_DEFAULT            0 // from export
#define MBED_CONF_QUECTEL_M26_BAUDRATE                    115200 // from export
#define MBED_CONF_QUECTEL_M26_PROVIDE_DEFAULT             0 // from export
#define MBED_CONF_QUECTEL_UG96_BAUDRATE                   115200 // from export
#define MBED_CONF_QUECTEL_UG96_PROVIDE_DEFAULT            0 // from export
#define MBED_CONF_RM1000_AT_BAUDRATE                      230400 // from export
#define MBED_CONF_RM1000_AT_PROVIDE_DEFAULT               0 // from export
#define MBED_CONF_RTOS_API_PRESENT                        1 // from export
#define MBED_CONF_RTOS_IDLE_THREAD_STACK_SIZE             512 // from export
#define MBED_CONF_RTOS_IDLE_THREAD_STACK_SIZE_DEBUG_EXTRA 128 // from export
#define MBED_CONF_RTOS_IDLE_THREAD_STACK_SIZE_TICKLESS_EXTRA 256 // from export
#define MBED_CONF_RTOS_MAIN_THREAD_STACK_SIZE             4096 // from export
#define MBED_CONF_RTOS_THREAD_STACK_SIZE                  4096 // from export
#define MBED_CONF_RTOS_TIMER_THREAD_STACK_SIZE            768 // from export
#define MBED_CONF_S2LP_PROVIDE_DEFAULT                    0 // from export
#define MBED_CONF_STM32_EMAC_ETH_RXBUFNB                  4 // from export
#define MBED_CONF_STM32_EMAC_ETH_TXBUFNB                  4 // from export
#define MBED_CONF_TARGET_BOOT_STACK_SIZE                  0x400 // from export
#define MBED_CONF_TARGET_CONSOLE_UART                     1 // from export
#define MBED_CONF_TARGET_DEEP_SLEEP_LATENCY               4 // from export
#define MBED_CONF_TARGET_INIT_US_TICKER_AT_BOOT           1 // from export
#define MBED_CONF_TARGET_LPTICKER_LPTIM                   1 // from export
#define MBED_CONF_TARGET_LPTICKER_LPTIM_CLOCK             1 // from export
#define MBED_CONF_TARGET_LPUART_CLOCK_SOURCE              USE_LPUART_CLK_LSE|USE_LPUART_CLK_PCLK1 // from export
#define MBED_CONF_TARGET_MPU_ROM_END                      0x0fffffff // from export
#define MBED_CONF_TARGET_TICKLESS_FROM_US_TICKER          0 // from export
#define MBED_CONF_TELIT_HE910_BAUDRATE                    115200 // from export
#define MBED_CONF_TELIT_HE910_PROVIDE_DEFAULT             0 // from export
#define MBED_CONF_TELIT_ME910_BAUDRATE                    115200 // from export
#define MBED_CONF_TELIT_ME910_PROVIDE_DEFAULT             0 // from export
#define NS_USE_EXTERNAL_MBED_TLS                          // defined by library:nanostack // from export
#define _RTE_                                             // defined by library:rtos // from export
#endif
