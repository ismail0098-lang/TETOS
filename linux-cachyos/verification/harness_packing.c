/*
 * CBMC Verification Harness for CachyOS POC Flag-to-Bitmask Packing
 *
 * Verifies that the highly optimized multiply-and-shift bit-packing algorithm
 * correctly converts a 64-byte flag array into a 64-bit mask.
 */

#include <assert.h>
#include <string.h>
#include <stdbool.h>

typedef unsigned char u8;
typedef unsigned long long u64;

#define POC_BYTE_EXTRACT 0x0101010101010101ULL
#define POC_BYTE_PACK    0x0102040810204080ULL

#define POC_BMP8(w, i) \
	((((w)[i] & POC_BYTE_EXTRACT) * POC_BYTE_PACK >> 56) << ((i) * 8))

static inline u64 poc_flags_to_u64(const u8 *flags)
{
	u64 w[8];

	/* Phase 1: snapshot shared cache line to stack */
	memcpy(w, flags, 64);

	/* Phase 2: pack stack-local copy into bitmask */
	return POC_BMP8(w, 0) | POC_BMP8(w, 1) | POC_BMP8(w, 2) | POC_BMP8(w, 3) |
	       POC_BMP8(w, 4) | POC_BMP8(w, 5) | POC_BMP8(w, 6) | POC_BMP8(w, 7);
}

int main() {
    u8 flags[64];

    // To ensure the LSB check is verified, let's assume each flag has only its LSB set (0 or 1),
    // which matches how the kernel writes them: `state > 0 ? 1 : 0`
    for (int i = 0; i < 64; i++) {
        __CPROVER_assume(flags[i] == 0 || flags[i] == 1);
    }

    u64 mask = poc_flags_to_u64(flags);

    // Assert that the packed bitmask corresponds exactly to the array
    for (int k = 0; k < 64; k++) {
        bool bit_is_set = (mask & (1ULL << k)) != 0;
        bool flag_is_set = flags[k] != 0;
        assert(bit_is_set == flag_is_set);
    }

    return 0;
}
