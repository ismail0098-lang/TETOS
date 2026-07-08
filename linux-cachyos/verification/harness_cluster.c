/*
 * CBMC Verification Harness for CachyOS POC Cluster Search
 *
 * Verifies mathematical safety, no out-of-bounds, and correctness of
 * CPU cluster search logic (poc_cluster_search).
 */

#include <assert.h>
#include <stdbool.h>

typedef unsigned int u32;
typedef unsigned long long u64;

#define unlikely(x) (x)
#define static_branch_likely(x) (1) // Force improved RR path for verification

// Static keys & globals mocked
bool sched_poc_rr_improved = true;
unsigned int poc_rr_counter = 0;

// Golden ratio multiplication for scrambling
#define POC_HASH_MULT 0x9E3779B9U
#define POC_SCRAMBLE(counter) ((u32)(counter) * POC_HASH_MULT)

// Lemire fastrange mapping
#define POC_FASTRANGE(seed, range) ((u32)(((u64)(seed) * (u32)(range)) >> 32))

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

// Mock structure for scheduler shared domain data
struct sched_domain_shared {
    u64 poc_cluster_mask[64];
};

/* --- Functions Under Test --- */

static inline int poc_select_rr_improved(
	int base, u64 mask, unsigned int counter)
{
	int total = hweight64(mask);

	if (total <= 2) {
		if ((total == 2) && (counter & 1))
			mask &= mask - 1;

		return base + POC_CTZ64(mask);
	}

	{
		u32 scrambled = POC_SCRAMBLE(counter);
		int pick = POC_FASTRANGE(scrambled, total);

		return base + POC_PTSELECT(mask, pick);
	}
}

static inline int poc_cluster_search(int base, int tgt_bit,
	struct sched_domain_shared *sd_share, u64 mask)
{
	u64 cls_idle = mask & sd_share->poc_cluster_mask[tgt_bit];

	if (!cls_idle)
		return -1;

	if (static_branch_likely(&sched_poc_rr_improved)) {
		/* Improved path: inc counter here so LV3 fallback sees fresh value */
		unsigned int counter = ++poc_rr_counter;
		return poc_select_rr_improved(base, cls_idle, counter);
	}

	/* Current strategy: ctz lowest-bit (no RR), unchanged */
	return base + POC_CTZ64(cls_idle);
}

/* --- CBMC Harness Entry Point --- */

int main() {
    int base;
    int tgt_bit;
    struct sched_domain_shared sd_share;
    u64 mask;

    // Make inputs symbolic
    __CPROVER_assume(base >= 0);
    // Limit base to prevent integer overflow in return value checks
    __CPROVER_assume(base < 100000);
    
    // Target bit must be within valid array bounds [0, 63]
    __CPROVER_assume(tgt_bit >= 0 && tgt_bit < 64);

    // Call cluster search
    int res = poc_cluster_search(base, tgt_bit, &sd_share, mask);

    /* --- Verification Assertions --- */

    u64 cls_mask = sd_share.poc_cluster_mask[tgt_bit];
    u64 cls_idle = mask & cls_mask;

    if (res >= 0) {
        // 1. Returned CPU must be within range [base, base + 63]
        assert(res >= base && res < base + 64);

        int bit = res - base;

        // 2. The selected CPU must indeed be idle in the mask
        assert((mask & (1ULL << bit)) != 0);

        // 3. The selected CPU must belong to the target CPU's cluster
        assert((cls_mask & (1ULL << bit)) != 0);
    } else {
        // If it returned -1, there must be no idle CPUs in the cluster
        assert(cls_idle == 0);
    }

    return 0;
}
