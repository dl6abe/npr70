// The firmware keeps a 64 bit microsecond counter (Timer) and reads it as int with read_us(): the lower 32 bits,
// negative after 2^31 us. The Chrono call elapsed_time().count() returns all 64 bits.
// Three kinds of sites exist in the firmware:
//   A  the result is stored in an unsigned variable          (e.g. unsigned int loc_time = GLOBAL_timer.read_us();)
//   B  the result is used in an expression with unsigned     (e.g. (GLOBAL_timer.read_us() - old) / 1000000)
//   C  the expression is compared or divided                 (B with a result that is used as a number)
// Rule checked here: with static_cast<int>() all sites give the old value for every counter value. Without the
// cast, A still gives the old value (the conversion to unsigned keeps the lower 32 bits) and B/C do not.
#include <stdint.h>
#include <stdio.h>

static int fails = 0;
#define CHECK(c, msg) do { if (!(c)) { printf("FAIL: %s\n", msg); fails++; } } while (0)

static int read_us_old(int64_t t) { return (int)(uint32_t)t; }   // lower 32 bits as int, like the old read_us()

int main()
{
	const int64_t counters[] = {0, 1000, 0x7FFFFFFFLL, 0x80000000LL, 0xFFFFFFFFLL, 0x100000000LL, 0x100000100LL,
	                            0x1FFFFFFFFLL, 0x2AAAAAAAAAALL};
	const unsigned olds[] = {0u, 100u, 1000u, 0x7FFFFFFFu, 0x80000000u, 0xFFFFFF00u, 0xFFFFFFFFu};
	int a_diff = 0, b_diff = 0, cast_diff = 0, n = 0;
	for (int64_t t : counters) {
		// A: store in unsigned
		unsigned a_old = read_us_old(t);
		unsigned a_cast = static_cast<int>(t);
		unsigned a_raw = t;
		CHECK(a_cast == a_old, "A with cast");
		if (a_raw != a_old) a_diff++;
		for (unsigned old : olds) {
			n++;
			// B: expression with an unsigned variable, result unsigned
			unsigned b_old = read_us_old(t) - old;
			unsigned b_cast = static_cast<int>(t) - old;
			CHECK(b_cast == b_old, "B with cast");
			// C: the expression itself is compared and divided
			bool c_old = (read_us_old(t) - old) > 1000000u;
			bool c_cast = (static_cast<int>(t) - old) > 1000000u;
			bool c_raw = (t - old) > 1000000;
			unsigned long long d_old = (unsigned)(read_us_old(t) - old) / 1000000u;
			long long d_raw = (t - old) / 1000000;
			CHECK(c_cast == c_old, "C compare with cast");
			if (c_raw != c_old) b_diff++;
			if ((unsigned long long)d_raw != d_old) cast_diff++;
		}
	}
	CHECK(a_diff == 0, "A without cast must equal the old value (conversion keeps the lower 32 bits)");
	CHECK(b_diff > 0, "B/C without cast must differ in at least one case (else the cast is not needed)");
	CHECK(cast_diff > 0, "division without cast must differ in at least one case");
	printf("%d cases, without cast: A differs %d times, compare differs %d times, division differs %d times\n",
	       n, a_diff, b_diff, cast_diff);
	if (fails) { printf("chrono host test FAILED\n"); return 1; }
	printf("chrono host test OK\n");
	return 0;
}
