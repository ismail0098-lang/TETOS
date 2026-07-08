/*
 * CBMC Verification Harness for CachyOS Sysctl Memory & I/O Interaction Model
 *
 * Models state transitions of memory allocation, kswapd reclaim, RT CPU starvation,
 * and the OOM killer loop under CachyOS kernel settings.
 */

#include <assert.h>
#include <stdbool.h>

typedef unsigned long long u64;

int main() {
    // Symbolic system memory state
    u64 total_ram;
    u64 free_ram;
    u64 file_cache;
    u64 anon_ram;
    u64 unreclaimable_slab;
    u64 zram_used;
    u64 zram_free_swap;

    // Symbolic environment state
    bool rt_tasks_busy;
    bool kswapd_boosted;

    // Sysctl parameters (constants)
    int swappiness = 100;
    bool sched_rt_runtime_unlimited = true;

    // Assume realistic bounds
    __CPROVER_assume(total_ram >= 1024 * 1024 * 1024ULL); // At least 1GB
    __CPROVER_assume(free_ram <= total_ram);
    __CPROVER_assume(file_cache + anon_ram + unreclaimable_slab + free_ram == total_ram);
    __CPROVER_assume(zram_used <= total_ram);
    __CPROVER_assume(zram_free_swap <= total_ram);

    // Page alignment invariants
    __CPROVER_assume((total_ram & 4095) == 0);
    __CPROVER_assume((free_ram & 4095) == 0);
    __CPROVER_assume((file_cache & 4095) == 0);
    __CPROVER_assume((anon_ram & 4095) == 0);
    __CPROVER_assume((unreclaimable_slab & 4095) == 0);
    __CPROVER_assume((zram_used & 4095) == 0);
    __CPROVER_assume((zram_free_swap & 4095) == 0);

    u64 min_free_watermark = total_ram >> 7; // ~0.8% of RAM

    // Force system to start under memory pressure
    __CPROVER_assume(free_ram < min_free_watermark);

    // --- Model reclaim_memory() ---
    bool reclaimed = false;
    
    // Check for kswapd starvation
    bool kswapd_starved = (sched_rt_runtime_unlimited && rt_tasks_busy && !kswapd_boosted);
    
    if (!kswapd_starved) {
        // Reclaim file cache
        if (file_cache > 0) {
            u64 reclaim_chunk = file_cache >> 3;
            if (reclaim_chunk > 0) {
                file_cache -= reclaim_chunk;
                free_ram += reclaim_chunk;
                reclaimed = true;
            }
        }
        
        // Swap anonymous memory
        if (!reclaimed && anon_ram > 0 && zram_free_swap > 0 && swappiness == 100) {
            u64 swap_chunk = anon_ram >> 3;
            if (swap_chunk > zram_free_swap) {
                swap_chunk = zram_free_swap;
            }
            if (swap_chunk > 0) {
                u64 zram_phys_used = swap_chunk >> 2;
                if (free_ram >= zram_phys_used) {
                    free_ram -= zram_phys_used;
                    zram_used += zram_phys_used;
                    zram_free_swap -= swap_chunk;
                    anon_ram -= swap_chunk;
                    free_ram += swap_chunk;
                    reclaimed = true;
                }
            }
        }
    }

    // --- Model run_oom_killer() ---
    bool oom_progress = false;
    if (!reclaimed) {
        if (anon_ram > 0) {
            u64 freed = anon_ram >> 1;
            anon_ram -= freed;
            free_ram += freed;
            oom_progress = true;
        } else if (file_cache > 0) {
            u64 freed = file_cache >> 1;
            file_cache -= freed;
            free_ram += freed;
            oom_progress = true;
        }
    }

    // --- Verify Safety Property ---
    if (!reclaimed && !oom_progress) {
        // If neither reclaim nor OOM killer made progress, it must be due to kswapd starvation
        // or the complete absence of reclaimable memory.
        assert(kswapd_starved || (anon_ram == 0 && file_cache == 0));
    }

    return 0;
}
