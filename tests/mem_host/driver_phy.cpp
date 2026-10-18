// Case 11: PHY self-heal. The real W5500_link_poll, W5500_re_configure and its periodic call run against a
// fake PHYCFGR register and a clock the test drives. The fake PHY has link only while the cable is plugged
// and the RST bit (0x80) of the last write is set.
#include "mbed.h"
#include "W5500.h"
#include "global_variables.h"
#include "Eth_IPv4.h"
#include <time.h>

Timer GLOBAL_timer;
LAN_conf_T LAN_conf_applied;
bool is_TDMA_master = false;
unsigned char is_telnet_routed = 0;
unsigned char CONF_Eth_mode = 7;
W5500_chip* W5500_p1;

static unsigned char phy_reg = 0xF8;   // last value written to PHYCFGR
static int cable = 1;
static unsigned char phy_value() { return (phy_reg & 0xFE) | ((cable && (phy_reg & 0x80)) ? 1 : 0); }
uint8_t W5500_read_byte(W5500_chip*, unsigned int addr, unsigned char) { return addr == 0x002E ? phy_value() : 0; }
void W5500_write_byte(W5500_chip*, unsigned int addr, unsigned char, uint8_t d) { if (addr == 0x002E) phy_reg = d; }
void W5500_write_long(W5500_chip*, unsigned int, unsigned char, const uint8_t*, int) {}
void IP_int2char(unsigned long int, unsigned char* d) { memset(d, 0, 4); }

#include "phy_part.inc"

static unsigned int now_us = 5000000;
static unsigned int next_periodic_us = 0;
static int periodic_on = 1;
// advance the fake clock in 25 ms steps; the periodic call runs every 2 s like in main()
static void run_s(W5500_chip* chip, int seconds) {
	unsigned int end = now_us + (unsigned int)seconds * 1000000u;
	while ((int)(end - now_us) > 0) {
		now_us += 25000;
		stub_timer_us = now_us;
		if (periodic_on && (int)(now_us - next_periodic_us) >= 0) { W5500_re_configure_periodic_call(chip); next_periodic_us = now_us + 2000000; }
		W5500_link_poll(chip);
	}
}
static int fails = 0;
static void check(const char* what, int cond) { if (!cond) { printf("MISMATCH: %s\n", what); fails++; } }

int main() {
	W5500_chip chip; SPI_F4HDK spi; DigitalOut cs;
	chip.spi_port = &spi; chip.cs = &cs;
	stub_timer_fixed = 1; stub_timer_us = now_us;

	run_s(&chip, 5);
	check("link up at start", phy_value() & 1);
	check("no heal, no reconf", W5500_heal_count() == 0 && W5500_reconf_runs() == 0);

	// A: normal re-configure: PHY held in reset for a few seconds, then released, no heal
	W5500_configured = 2; W5500_re_configure(0x08);
	check("reconf counted", W5500_reconf_runs() == 1 && W5500_reconf_why() == 0x08 && W5500_reconf_state() == 2);
	run_s(&chip, 10);
	check("A: back to ready", W5500_reconf_state() == 1);
	check("A: link up again", phy_value() & 1);
	check("A: no heal", W5500_heal_count() == 0);

	// B: the periodic call stops while the PHY is held in reset: forced to end after 15 s
	W5500_re_configure(0x01);
	periodic_on = 1; run_s(&chip, 3);              // state 2 -> 3, PHY in reset
	check("B: PHY held", !(phy_reg & 0x80) && !(phy_value() & 1));
	periodic_on = 0; run_s(&chip, 10);                // 13 s since the request
	check("B: not yet forced at 13 s", W5500_heal_count() == 0 && !(phy_value() & 1));
	run_s(&chip, 4);                                  // 17 s
	check("B: forced to ready", W5500_reconf_state() == 1 && W5500_heal_count() == 1);
	check("B: link up after release", phy_value() & 1);
	periodic_on = 1;

	// C: PHY held in reset with state ready (nobody releases it): heal after 30 s
	unsigned int h = W5500_heal_count();
	phy_reg = 0x78;
	run_s(&chip, 25);
	check("C: no heal before 30 s", W5500_heal_count() == h && !(phy_value() & 1));
	run_s(&chip, 8);
	check("C: released by heal", W5500_heal_count() == h + 1 && (phy_value() & 1));
	check("C: link down time is 0 when up", W5500_link_down_s() == 0);

	// D: cable unplugged for 100 s: PHY reset about every 30 s, link back when plugged again
	h = W5500_heal_count();
	cable = 0; run_s(&chip, 100);
	check("D: 3 resets in 100 s", W5500_heal_count() == h + 3);
	check("D: link down time counted", W5500_link_down_s() >= 99);
	cable = 1; run_s(&chip, 3);
	check("D: link up after plugging", phy_value() & 1);
	h = W5500_heal_count();
	run_s(&chip, 300);
	check("D: no heal while link is up", W5500_heal_count() == h);

	// E: no heal during a short outage (cable pulled for 20 s)
	cable = 0; run_s(&chip, 20); cable = 1; run_s(&chip, 5);
	check("E: short outage not healed", W5500_heal_count() == h && (phy_value() & 1));

	printf(fails ? "FAIL\n" : "PASS\n");
	return fails ? 1 : 0;
}
