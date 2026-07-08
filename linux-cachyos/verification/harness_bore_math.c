#include <assert.h>

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long long u64;
typedef long long s64;
typedef int s32;
typedef _Bool bool;

#define true 1
#define false 0
#define unlikely(x) __builtin_expect(!!(x), 0)

u8 sched_burst_penalty_offset = 24;
u32 sched_burst_penalty_scale = 1536;
u8 sched_burst_smoothness = 1;

#define MAX_BURST_PENALTY ((40U << 8) - 1)

static inline u32 log2p1_u64_u32fp(u64 v, u8 fp) {
	if (unlikely(!v)) return 0;
	int clz = __builtin_clzll(v);
	int exponent = 64 - clz;
	u32 mantissa = (u32)((v << clz) << 1 >> (64 - fp));
	return exponent << fp | mantissa;
}

static inline u32 calc_burst_penalty(u64 burst_time) {
	u32 greed = log2p1_u64_u32fp(burst_time, 8),
		tolerance = (u32)sched_burst_penalty_offset << 8;
	s32 diff = (s32)(greed - tolerance);
	u32 penalty = diff & ~(diff >> 31);
	u32 scaled_penalty = penalty * sched_burst_penalty_scale >> 10;
	s32 overflow = scaled_penalty - MAX_BURST_PENALTY;
	return scaled_penalty - (overflow & ~(overflow >> 31));
}

static inline u32 binary_smooth(u32 new_val, u32 old_val) {
	u32 is_growing = (new_val > old_val);
	u32 increment = (new_val - old_val) * is_growing;
	u32 shift = sched_burst_smoothness;
	u32 smoothed = old_val + ((increment + (1U << shift) - 1) >> shift);
	return (new_val & ~(-is_growing)) | (smoothed & (-is_growing));
}

int main() {
    u64 burst_time;
    u32 old_val;
    u8 fp;

    // Constrain fp to avoid undefined behavior of shifts by >= 64 or negative
    __CPROVER_assume(fp >= 1 && fp <= 16);
    __CPROVER_assume(sched_burst_smoothness >= 0 && sched_burst_smoothness <= 31);
    __CPROVER_assume(sched_burst_penalty_offset >= 0 && sched_burst_penalty_offset <= 63);
    __CPROVER_assume(sched_burst_penalty_scale >= 0 && sched_burst_penalty_scale <= 4095);

    // Call log2p1_u64_u32fp with non-deterministic input
    u32 l = log2p1_u64_u32fp(burst_time, fp);

    // Verify properties of log2p1
    if (burst_time > 0) {
        assert(l > 0);
    } else {
        assert(l == 0);
    }

    // Call calc_burst_penalty
    u32 penalty = calc_burst_penalty(burst_time);
    assert(penalty <= MAX_BURST_PENALTY);

    // Call binary_smooth
    u32 smoothed = binary_smooth(penalty, old_val);
    if (penalty <= old_val) {
        assert(smoothed == penalty);
    } else {
        assert(smoothed >= old_val && smoothed <= penalty);
    }

    return 0;
}
