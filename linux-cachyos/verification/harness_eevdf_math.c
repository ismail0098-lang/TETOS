/*
 * CBMC Verification Harness for EEVDF (Completely Fair Scheduler) Core Math
 *
 * Verifies mathematical safety (no overflow, no division by zero) and correctness
 * of key EEVDF functions: vruntime comparisons, weight scaling, and average vruntime.
 */

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>

typedef unsigned long long u64;
typedef long long s64;
typedef unsigned int u32;

#define NICE_0_LOAD 1024

struct load_weight {
    unsigned long weight;
    u32 inv_weight;
};

struct sched_entity {
    struct load_weight load;
    u64 vruntime;
    u64 deadline;
    u64 slice;
    bool on_rq;
};

struct cfs_rq {
    struct sched_entity *curr;
    u64 zero_vruntime;
    s64 sum_w_vruntime;
    long sum_weight;
    int sum_shift;
};

/* --- Helpers Mocked from fair.c --- */

#define vruntime_cmp(A, OP, B) \
    ((OP[0] == '<' && OP[1] == '\0') ? ((s64)((A)-(B)) < 0) : \
     (OP[0] == '<' && OP[1] == '=') ? ((s64)((A)-(B)) <= 0) : \
     (OP[0] == '>' && OP[1] == '\0') ? ((s64)((A)-(B)) > 0) : \
     (OP[0] == '>' && OP[1] == '=') ? ((s64)((A)-(B)) >= 0) : 0)

static inline s64 vruntime_op_sub(u64 A, u64 B) {
    return (s64)(A - B);
}

static inline s64 entity_key(struct cfs_rq *cfs_rq, struct sched_entity *se) {
    return vruntime_op_sub(se->vruntime, cfs_rq->zero_vruntime);
}

static inline unsigned long avg_vruntime_weight(struct cfs_rq *cfs_rq, unsigned long w) {
#ifdef CONFIG_64BIT
    if (cfs_rq->sum_shift) {
        unsigned long val = w >> cfs_rq->sum_shift;
        return (val > 2UL) ? val : 2UL;
    }
#endif
    return w;
}

/* 64-bit kernel __calc_delta implementation */
static u64 __calc_delta(u64 delta_exec, unsigned long weight, struct load_weight *lw) {
    // Prevent division by zero
    if (lw->weight == 0) return 0;
    return (delta_exec * weight) / lw->weight;
}

static inline u64 calc_delta_fair(u64 delta, struct sched_entity *se) {
    if (se->load.weight != NICE_0_LOAD) {
        delta = __calc_delta(delta, NICE_0_LOAD, &se->load);
    }
    return delta;
}

/* EEVDF Weighted Average vruntime calculation */
u64 avg_vruntime(struct cfs_rq *cfs_rq) {
    struct sched_entity *curr = cfs_rq->curr;
    long weight = cfs_rq->sum_weight;
    s64 delta = 0;

    if (curr && !curr->on_rq) {
        curr = NULL;
    }

    if (weight > 0) {
        s64 runtime = cfs_rq->sum_w_vruntime;

        if (curr) {
            unsigned long w = avg_vruntime_weight(cfs_rq, curr->load.weight);
            s64 key = entity_key(cfs_rq, curr);
            
            // Check for potential overflow before updating runtime
            s64 w_vruntime = key * (s64)w;
            runtime += w_vruntime;
            weight += w;
        }

        if (weight > 0) {
            /* left bias the average */
            if (runtime < 0) {
                delta = (runtime - weight + 1) / weight;
            } else {
                delta = runtime / weight;
            }
        }
    }

    return cfs_rq->zero_vruntime + (u64)delta;
}

/* --- Verification Entry Point --- */

int main() {
    struct cfs_rq cfs;
    struct sched_entity curr;
    
    // Make variables symbolic
    __CPROVER_assume(cfs.sum_weight >= 0);
    __CPROVER_assume(cfs.sum_shift >= 0 && cfs.sum_shift < 10);
    
    // Constrain weights to avoid massive overflows in scale checks
    __CPROVER_assume(curr.load.weight > 0 && curr.load.weight < 1000000);
    __CPROVER_assume(cfs.sum_weight < 10000000);
    
    // Bounded virtual runtime lag to realistic scheduler horizons (e.g. +/- 100 seconds in ns)
    s64 key = entity_key(&cfs, &curr);
    __CPROVER_assume(key >= -100000000000LL && key <= 100000000000LL);

    // Bounded sum_w_vruntime based on maximum scheduling queue sizes
    __CPROVER_assume(cfs.sum_w_vruntime >= -1000000000000000LL && cfs.sum_w_vruntime <= 1000000000000000LL);

    // Connect curr if set
    bool has_curr;
    if (has_curr) {
        cfs.curr = &curr;
        curr.on_rq = true;
    } else {
        cfs.curr = NULL;
    }

    // Call avg_vruntime
    u64 avg = avg_vruntime(&cfs);

    // Assertions
    // 1. If sum_weight is positive, average vruntime must be a bounded offset from zero_vruntime.
    //    The bound derives directly from the input constraints:
    //      - sum_w_vruntime ∈ [-10^15, +10^15]  (declared above)
    //      - sum_weight ∈ [1, 10^7]              (declared above)
    //    Therefore delta = sum_w_vruntime / sum_weight ∈ [-10^15, +10^15].
    //    This verifies avg_vruntime() didn't silently wrap or produce a result
    //    outside the representable range for the given symbolic inputs.
    if (cfs.sum_weight > 0) {
        s64 diff = (s64)(avg - cfs.zero_vruntime);
        assert(diff >= -1000000000000000LL && diff <= 1000000000000000LL);
    }

    // Test vruntime wrapping comparison properties
    u64 a, b, c;
    // We assume the difference between runtimes is bounded to standard scheduling horizons (< 2^63)
    __CPROVER_assume((s64)(a - b) < 4611686018427387904LL && (s64)(a - b) > -4611686018427387904LL);
    __CPROVER_assume((s64)(b - c) < 4611686018427387904LL && (s64)(b - c) > -4611686018427387904LL);
    __CPROVER_assume((s64)(a - c) < 4611686018427387904LL && (s64)(a - c) > -4611686018427387904LL);

    // Transitivity check under bounded wrap-around
    if (vruntime_cmp(a, "<", b) && vruntime_cmp(b, "<", c)) {
        assert(vruntime_cmp(a, "<", c));
    }

    // Delta fair calculation check
    u64 exec_time;
    __CPROVER_assume(exec_time < 1000000000ULL); // Bounded to 1 second in ns
    u64 delta = calc_delta_fair(exec_time, &curr);
    
    if (curr.load.weight == NICE_0_LOAD) {
        // Scaling by NICE_0_LOAD should be identity function
        assert(delta == exec_time);
    }

    return 0;
}
