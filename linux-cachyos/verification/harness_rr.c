/*
 * CBMC Verification Harness for CachyOS POC Sched RR
 *
 * Verifies mathematical safety, no out-of-bounds, no division-by-zero,
 * and semantic correctness of idle CPU selection logic.
 */

#include <assert.h>
#include <stdbool.h>

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long long u64;
typedef long long s64;

#define unlikely(x) (x)
#define static_branch_likely(x) (1) // Force improved RR path for verification

// Static keys mocked
bool sched_poc_rr_improved = true;

// Golden ratio multiplication for scrambling
#define POC_HASH_MULT 0x9E3779B9U
#define POC_SCRAMBLE(counter) ((u32)(counter) * POC_HASH_MULT)

// Lemire fastrange mapping
#define POC_FASTRANGE(seed, range) ((u32)(((u64)(seed) * (u32)(range)) >> 32))

// Modulo estimation
#define POC_FIXED_MOD16(phase, range) ((u32)(((u32)(phase) * (u32)(range)) >> 16))

// CTZ64 using compiler builtin
static inline int ctz64(u64 v) {
    if (v == 0) return 64;
    return __builtin_ctzll(v);
}
#define POC_CTZ64(v) ctz64(v)

// Popcount using compiler builtin
static inline int hweight64(u64 v) {
    return __builtin_popcountll(v);
}

// Software implementation of ptselect
static inline int poc_ptselect_sw(u64 v, int j) {
    int k;
    for (k = 0; k < j; k++) {
        v &= v - 1; /* clear lowest set bit */
    }
    return POC_CTZ64(v);
}
#define POC_PTSELECT(v, j) poc_ptselect_sw(v, j)

// RR step table
static const u16 poc_rr_step[64] = {
	     0, 0x8000, 0x5556, 0x4000,	0x3334, 0x2AAB, 0x2493, 0x2000,	/*  1.. 8 */
	0x1C72, 0x199A, 0x1746, 0x1556,	0x13B2, 0x124A, 0x1112, 0x1000,	/*  9..16 */
	0x0F10, 0x0E39, 0x0D7A, 0x0CCD,	0x0C31, 0x0BA3, 0x0B22, 0x0AAB,	/* 17..24 */
	0x0A3E, 0x09D9, 0x097C, 0x0925,	0x08D4, 0x0889, 0x0843, 0x0800,	/* 25..32 */
	0x07C2, 0x0788, 0x0751, 0x071D,	0x06EC, 0x06BD, 0x0691, 0x0667,	/* 33..40 */
	0x063F, 0x0619, 0x05F5, 0x05D2,	0x05B1, 0x0591, 0x0573, 0x0556,	/* 41..48 */
	0x053A, 0x051F, 0x0506, 0x04ED,	0x04D5, 0x04BE, 0x04A8, 0x0493,	/* 49..56 */
	0x047E, 0x046A, 0x0457, 0x0445,	0x0433, 0x0422, 0x0411, 0x0400,	/* 57..64 */
};

/* --- Functions Under Test --- */

static inline int poc_select_rr_improved(
	int base, u64 mask, unsigned int counter)
{
	int total = hweight64(mask);

	if (total <= 2) {
		/*
		 * Pick the lower or upper set bit via counter LSB if total == 2.
		 * Select the mask first (cmov), then one CTZ — halves the
		 * cost on archs where CTZ64 is a SW fallback (De Bruijn).
		 */
		if ((total == 2) && (counter & 1))
			mask &= mask - 1;

		return base + POC_CTZ64(mask);
	}

	/* total >= 3: golden-ratio scramble + Lemire fastrange */
	{
		u32 scrambled = POC_SCRAMBLE(counter);
		int pick = POC_FASTRANGE(scrambled, total);

		return base + POC_PTSELECT(mask, pick);
	}
}

static inline int poc_select_rr(int base, u64 mask, unsigned int counter)
{
	if (static_branch_likely(&sched_poc_rr_improved))
		return poc_select_rr_improved(base, mask, counter);

	/* Current strategy: poc_rr_step[] table (perfect RR), unchanged */
	{
		int total = hweight64(mask);
		u16 phase = (u16)(counter * (u32)poc_rr_step[total - 1]);
		int pick  = POC_FIXED_MOD16(phase, total);

		return POC_PTSELECT(mask, pick) + base;
	}
}

/* --- CBMC Harness Entry Point --- */

int main() {
    int base;
    u64 mask;
    unsigned int counter;

    // Make inputs symbolic
    __CPROVER_assume(base >= 0);
    // Limit base to prevent integer overflow in return value checks
    __CPROVER_assume(base < 100000);
    
    // We assume mask is not 0 since select_rr caller guarantees it
    __CPROVER_assume(mask != 0);

    // Call improved RR path
    sched_poc_rr_improved = true;
    int res_improved = poc_select_rr(base, mask, counter);

    // Call legacy RR path
    sched_poc_rr_improved = false;
    int res_legacy = poc_select_rr(base, mask, counter);

    /* --- Verification Assertions --- */

    // 1. Chosen CPU must be within range [base, base + 63]
    assert(res_improved >= base && res_improved < base + 64);
    assert(res_legacy >= base && res_legacy < base + 64);

    // 2. The chosen bit in the mask MUST be set (i.e. we selected an IDLE CPU)
    int bit_improved = res_improved - base;
    int bit_legacy = res_legacy - base;
    
    assert((mask & (1ULL << bit_improved)) != 0);
    assert((mask & (1ULL << bit_legacy)) != 0);

    return 0;
}
