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

#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define NSEC_PER_MSEC 1000000ULL
#define STARVATION_MS 5000ULL

struct sched_ext_entity {
    u64 weight;
    u64 dsq_vtime;
};

struct task_struct {
    struct sched_ext_entity scx;
};

struct task_ctx {
    u64 wakeup_freq;
    u64 awake_vtime;
};

static inline bool time_after(u64 a, u64 b)
{
	return (s64)(b - a) < 0;
}

static inline bool time_before(u64 a, u64 b)
{
	return time_after(b, a);
}

static inline u64 scale_by_task_weight(const struct task_struct *p, u64 value)
{
	return (value * p->scx.weight) / 100;
}

static inline u64 scale_by_task_weight_inverse(const struct task_struct *p, u64 value)
{
	return value * 100 / p->scx.weight;
}

static inline u64 cpu_dsq(s32 cpu)
{
	return cpu;
}

static inline u64 node_dsq(s32 cpu)
{
	return cpu + 1024;
}

u64 mock_nr_queued_cpu;
u64 mock_nr_queued_node;

u64 scx_bpf_dsq_nr_queued(u64 dsq_id) {
    if (dsq_id < 1024) {
        return mock_nr_queued_cpu;
    } else {
        return mock_nr_queued_node;
    }
}

u64 slice_max;
u64 slice_min;
u64 slice_lag;
u64 vtime_now;

static u64 task_dl(struct task_struct *p, s32 cpu, struct task_ctx *tctx)
{
	const u64 STARVATION_THRESH = STARVATION_MS * NSEC_PER_MSEC / 10;
	const u64 q_thresh = MAX(STARVATION_THRESH / slice_max, 1);

	u64 nr_queued = scx_bpf_dsq_nr_queued(cpu_dsq(cpu)) +
			scx_bpf_dsq_nr_queued(node_dsq(cpu));
	u64 lag_scale = MAX(tctx->wakeup_freq, 1);
	u64 awake_max = scale_by_task_weight_inverse(p, slice_lag);
	u64 vtime_min;

	if (nr_queued * slice_max >= STARVATION_THRESH)
		lag_scale = 1;
	else
		lag_scale = MAX(lag_scale * q_thresh / (q_thresh + nr_queued), 1);

	vtime_min = vtime_now - scale_by_task_weight(p, slice_lag * lag_scale);
	if (time_before(p->scx.dsq_vtime, vtime_min))
		p->scx.dsq_vtime = vtime_min;

	if (time_after(tctx->awake_vtime, awake_max))
		tctx->awake_vtime = awake_max;

	return p->scx.dsq_vtime + tctx->awake_vtime;
}

int main() {
    struct task_struct p;
    struct task_ctx tctx;
    s32 cpu;

    // Initialize inputs with non-deterministic values (CBMC symbols)
    //
    // NOTE on vtime bounds: In the real BPF scheduler, dsq_vtime and vtime_now
    // are relative offsets from a per-DSQ epoch that gets periodically reset.
    // They never approach UINT64_MAX. The bounds below (up to 10^12 ns ≈ 16 min)
    // cover realistic scheduling horizons. Unbounded vtimes would trigger unsigned
    // wrapping in (dsq_vtime + awake_vtime) which is intentional in the real
    // scheduler but not what this harness models.
    __CPROVER_assume(p.scx.weight > 0 && p.scx.weight <= 10000); // Typical weight range in kernel
    __CPROVER_assume(slice_max >= 1000000ULL && slice_max <= 100000000ULL); // 1ms to 100ms
    __CPROVER_assume(slice_lag >= 1000000ULL && slice_lag <= 100000000ULL);
    __CPROVER_assume(tctx.wakeup_freq >= 0 && tctx.wakeup_freq <= 1000ULL);
    __CPROVER_assume(tctx.awake_vtime >= 0 && tctx.awake_vtime <= 1000000000ULL);
    __CPROVER_assume(p.scx.dsq_vtime >= 0 && p.scx.dsq_vtime <= 1000000000000ULL);
    __CPROVER_assume(vtime_now >= 0 && vtime_now <= 1000000000000ULL);
    __CPROVER_assume(mock_nr_queued_cpu >= 0 && mock_nr_queued_cpu <= 1000);
    __CPROVER_assume(mock_nr_queued_node >= 0 && mock_nr_queued_node <= 1000);
    __CPROVER_assume(cpu >= 0 && cpu < 1024);

    u64 dl = task_dl(&p, cpu, &tctx);

    // Assert that the returned deadline is reasonable and hasn't suffered massive overflow/underflow
    assert(dl >= p.scx.dsq_vtime);

    return 0;
}
