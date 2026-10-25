// libFuzzer target for the SNMP agent: the input is one SNMP message (UDP payload).
// Byte 0, bit 0 selects master (0) or client (1) state, the rest is the message.
// The globals and the W5500 stubs come from the SNMP host test driver (its main is renamed).
#define main driver_main
#include "driver.cpp"
#undef main

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size < 1 || size > sizeof g_in - 8) return 0;
    is_TDMA_master = !(data[0] & 1);
    my_radio_client_ID = is_TDMA_master ? 0x7E : 1;
    g_in_len = 0;
    for (int i = 0; i < 8; i++) g_in[g_in_len++] = 0;     // W5500 UDP header (IP, port, length)
    memcpy(g_in + g_in_len, data + 1, size - 1);
    g_in_len += size - 1;
    g_out_len = 0;
    snmp_loop(&g_chip);
    return 0;
}
