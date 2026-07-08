/*
 * CBMC Verification Harness for CachyOS Sysctl Memory & I/O Interaction Model
 *
 * Models state transitions of memory allocation, kswapd reclaim, RT CPU starvation,
 * and the OOM killer loop under:
 * - vm.swappiness = 100
 * - vm.vfs_cache_pressure = 50
 * - kernel.unprivileged_userns_clone = 1
 * - kernel.sched_rt_runtime_us = -1
 */

#include <assert.h>
#include <stdbool.h>

typedef unsigned long long u64;

// System memory state
u64 total_ram;
u64 free_ram;
u64 file_cache;
u64 anon_ram;
u64 unreclaimable_slab;
u64 zram_used;
u64 zram_free_swap;

// Sysctl parameters
int swappiness = 100;
int vfs_cache_pressure = 50;
bool unprivileged_userns_clone = true;
bool sched_rt_runtime_unlimited = true;

// Environment state
bool rt_tasks_busy;
bool kswapd_boosted;

// Watermarks
u64 min_free_watermark;

// Initialize system state symbolicaly
void init_state() {
    __CPROVER_assume(total_ram >= 1024 * 1024 * 1024ULL); // At least 1GB
    __CPROVER_assume(free_ram <= total_ram);
    __CPROVER_assume(file_cache + anon_ram + unreclaimable_slab + free_ram == total_ram);
    __CPROVER_assume(zram_used <= total_ram);
    __CPROVER_assume(zram_free_swap <= total_ram);
    
    min_free_watermark = total_ram / 100; // 1% of RAM
}

// Model process allocating memory
bool allocate_memory(u64 size, bool is_ns_allocation) {
    if (free_ram >= size) {
        free_ram -= size;
        if (is_ns_allocation) {
            // Namespace / mount allocations go to un-reclaimable slab
            unreclaimable_slab += size;
        } else {
            anon_ram += size;
        }
        return true;
    }
    return false;
}

// Model kernel page/slab reclaim (kswapd / direct reclaim)
bool reclaim_memory() {
    // If RT tasks run infinitely and kswapd is not boosted/RT,
    // kswapd is starved and cannot reclaim memory!
    if (sched_rt_runtime_unlimited && rt_tasks_busy && !kswapd_boosted) {
        return false; // Starvation / Reclaim fails
    }

    bool progress = false;
    
    // 1. Reclaim file cache
    if (file_cache > 0) {
        // vfs_cache_pressure = 50 means we reclaim directory/inode metadata slower,
        // so file cache reclaim is preferred.
        u64 reclaim_chunk = file_cache / 10;
        if (reclaim_chunk > 0) {
            file_cache -= reclaim_chunk;
            free_ram += reclaim_chunk;
            progress = true;
        }
    }

    // 2. Swap out anonymous memory to zram
    if (anon_ram > 0 && zram_free_swap > 0 && swappiness == 100) {
        u64 swap_chunk = anon_ram / 10;
        if (swap_chunk > zram_free_swap) {
            swap_chunk = zram_free_swap;
        }
        
        if (swap_chunk > 0) {
            // zstd compression ratio ~3:1, zram physical overhead
            u64 zram_phys_used = swap_chunk / 3;
            if (free_ram >= zram_phys_used) {
                free_ram -= zram_phys_used;
                zram_used += zram_phys_used;
                zram_free_swap -= swap_chunk;
                
                anon_ram -= swap_chunk;
                free_ram += swap_chunk;
                progress = true;
            }
        }
    }

    return progress;
}

// Model Out-Of-Memory (OOM) Killer execution
bool run_oom_killer() {
    // OOM killer attempts to free anonymous memory or file cache from killed tasks.
    // It cannot reclaim memory locked in kernel slabs (like namespace descriptors).
    if (anon_ram > 0) {
        u64 freed = anon_ram / 2;
        anon_ram -= freed;
        free_ram += freed;
        return true; // Progress made
    }
    if (file_cache > 0) {
        u64 freed = file_cache / 2;
        file_cache -= freed;
        free_ram += freed;
        return true;
    }
    return false; // Lockup! No progress possible (only unreclaimable slab left)
}

int main() {
    init_state();

    // Verify system safety under continuous memory pressure
    int loops = 0;
    while (free_ram < min_free_watermark && loops < 5) {
        loops++;
        
        bool reclaimed = reclaim_memory();
        if (!reclaimed) {
            // Reclaim failed, trigger OOM killer
            bool oom_progress = run_oom_killer();
            if (!oom_progress) {
                // If OOM killer cannot reclaim anything, we have an inescapable lockup!
                
                // ASSERTION: System should not lock up under memory pressure.
                // This fails if all memory gets converted to unprivileged user namespace slabs,
                // or if kswapd is completely starved.
                assert(false);
            }
        }
    }

    return 0;
}
