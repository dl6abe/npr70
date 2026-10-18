// Replacement for HAL_GetTick(), linked with -Wl,--wrap,HAL_GetTick (build_port.sh, HAL_TICK=1 or 2).
//
// Since mbed-os 5.9.1 (commit f3ea877f3a "Replace HAL_GetTick") the HAL time comes from
// ticker_read_us(get_us_ticker_data()) / 1000. With it the firmware loses most of its ping traffic
// and the W5500 reports phantom data (NETWORK_DEBUG_LOG.md 2.72). The HAL calls HAL_GetTick() for
// its timeouts, our code calls HAL functions from interrupts. These variants read the time without
// the ticker queue:
//   1  milliseconds from the raw 32 bit microsecond counter. The wraps (every 71.6 minutes) are
//      counted here, so the function must be called at least once per wrap (the SPI code does).
//   2  tick count of the RTOS kernel plus an offset (continuous with the old time) once the kernel
//      runs, the original function before.
// Without NPR_HAL_TICK this file is empty.
#include "mbed.h"

#if defined(NPR_HAL_TICK) && NPR_HAL_TICK

#include "hal/us_ticker_api.h"
#include "cmsis_os2.h"

extern "C" uint32_t __real_HAL_GetTick(void);

#if NPR_HAL_TICK == 1

extern "C" uint32_t __wrap_HAL_GetTick(void)
{
    static uint32_t last_us = 0;
    static uint32_t wraps = 0;
    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    uint32_t now = us_ticker_read();
    if (now < last_us) {
        wraps++;
    }
    last_us = now;
    uint64_t us = ((uint64_t)wraps << 32) | now;
    __set_PRIMASK(primask);
    return (uint32_t)(us / 1000);
}

#elif NPR_HAL_TICK == 2

extern "C" uint32_t __wrap_HAL_GetTick(void)
{
    static uint32_t offset = 0;
    static bool have_offset = false;
    if (osKernelGetState() != osKernelRunning) {
        return __real_HAL_GetTick();
    }
    uint32_t ticks = osKernelGetTickCount();
    if (!have_offset) {
        offset = __real_HAL_GetTick() - ticks;
        have_offset = true;
    }
    return ticks + offset;
}

#endif
#endif
