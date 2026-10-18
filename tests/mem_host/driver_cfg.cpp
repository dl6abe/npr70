// Case 14: the callsign is copied with strncpy(..., 15) into a field of 16 bytes (config_flash.cpp, reading
// the flash and writing it back). The two lines of the real source, each followed by the explicit end byte,
// are run on 16 bytes without any 0 (NETWORK_DEBUG_LOG.md 2.36 point 11). The result must end with 0 at
// byte 15 and must not read or write outside the 16 bytes.
#include "mbed.h"

struct npr_config_stub { char callsign[16]; };
static char CONF_radio_my_callsign[16];

int main() {
	int bad = 0;
	for (int round = 0; round < 2; round++) {
		npr_config_stub* d = (npr_config_stub*)malloc(sizeof(npr_config_stub));   // exact size for ASan
		npr_config_stub* data_r = d;
		if (round == 0) {          // read: 16 characters from the flash, no end byte
			memset(d->callsign, 'A', 16);
			memset(CONF_radio_my_callsign, 'Z', 16);
#include "cfg_read.inc"
			if (CONF_radio_my_callsign[15] != 0 || CONF_radio_my_callsign[14] != 'A') { printf("MISMATCH read: byte 15 = %d\n", CONF_radio_my_callsign[15]); bad = 1; }
		} else {                   // write: 16 characters in RAM, no end byte
			memset(CONF_radio_my_callsign, 'B', 16);
			memset(d->callsign, 'Z', 16);
#include "cfg_write.inc"
			if (data_r->callsign[15] != 0 || data_r->callsign[14] != 'B') { printf("MISMATCH write: byte 15 = %d\n", data_r->callsign[15]); bad = 1; }
		}
		free(d);
	}
	printf("%s\n", bad ? "failed" : "callsign is terminated at byte 15 in both directions");
	return bad;
}
