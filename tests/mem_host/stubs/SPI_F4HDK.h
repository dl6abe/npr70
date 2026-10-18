#pragma once
// Stub of source/SPI_F4HDK.h: the host tests only need the type name (W5500.h includes it).
#include "mbed.h"
struct SPI_F4HDK : SPI { int transfer_2(const void*, int, void*, int); };
