/*
 * CBMC Verification Harness for zram-generator Memory Allocation Math
 *
 * Verifies that zram-generator size calculation logic (converting MemTotal from KiB
 * to MiB, evaluating scaling expressions, and writing bytes to /sys/block/zramX/disksize)
 * is safe from integer overflows, underflows, and truncations to 0.
 */

#include <assert.h>
#include <stdbool.h>

typedef unsigned long long u64;
typedef unsigned int u32;

#define KB_TO_MB(kb) ((kb) / 1024ULL)
#define MB_TO_BYTES(mb) ((mb) * 1024ULL * 1024ULL)

// Mimics the parsing and evaluation logic in rust's zram-generator
u64 zram_size_bytes_expr_ram(u64 mem_total_kib) {
    u64 ram_mib = KB_TO_MB(mem_total_kib);
    
    // Config: zram-size = ram
    u64 zram_size_mib = ram_mib; 
    
    // Conversion to bytes: zram_size_mib * 1024 * 1024
    // Using 64-bit math prevents overflow on large RAM configurations (e.g., servers with TBs of RAM)
    u64 size_bytes = MB_TO_BYTES(zram_size_mib);
    return size_bytes;
}

u64 zram_size_bytes_expr_half_ram(u64 mem_total_kib) {
    u64 ram_mib = KB_TO_MB(mem_total_kib);
    
    // Config: zram-size = ram / 2
    u64 zram_size_mib = ram_mib / 2;
    
    u64 size_bytes = MB_TO_BYTES(zram_size_mib);
    return size_bytes;
}

u64 zram_size_bytes_expr_min_capped(u64 mem_total_kib, u64 cap_mib) {
    u64 ram_mib = KB_TO_MB(mem_total_kib);
    
    // Config: zram-size = min(ram, cap_mib)
    u64 zram_size_mib = (ram_mib < cap_mib) ? ram_mib : cap_mib;
    
    u64 size_bytes = MB_TO_BYTES(zram_size_mib);
    return size_bytes;
}

u64 zram_size_bytes_expr_aggressive(u64 mem_total_kib) {
    u64 ram_mib = KB_TO_MB(mem_total_kib);
    
    // Config: zram-size = ram * 2 (e.g. double RAM logical swap capacity)
    u64 zram_size_mib = ram_mib * 2;
    
    u64 size_bytes = MB_TO_BYTES(zram_size_mib);
    return size_bytes;
}

int main() {
    u64 mem_total_kib;
    
    // Limit memory total to realistic capacities: from 256MB to 16TB of RAM
    __CPROVER_assume(mem_total_kib >= 256 * 1024ULL); 
    __CPROVER_assume(mem_total_kib <= 16ULL * 1024ULL * 1024ULL * 1024ULL); // 16 Terabytes in KiB

    // 1. Verify "zram-size = ram"
    u64 size_bytes_ram = zram_size_bytes_expr_ram(mem_total_kib);
    // Ensure size_bytes is not 0 (no integer truncation to 0 for valid memory totals)
    assert(size_bytes_ram > 0);
    // Ensure no wrapping overflow occurred (must be proportional to input)
    assert(size_bytes_ram >= mem_total_kib * 1000); // 1 KiB is 1024 bytes, so bytes >= kib * 1000

    // 2. Verify "zram-size = ram / 2"
    u64 size_bytes_half = zram_size_bytes_expr_half_ram(mem_total_kib);
    assert(size_bytes_half > 0);
    assert(size_bytes_half <= size_bytes_ram);

    // 3. Verify "zram-size = min(ram, 4096)"
    u64 cap_mib = 4096; // 4 GiB
    u64 size_bytes_capped = zram_size_bytes_expr_min_capped(mem_total_kib, cap_mib);
    assert(size_bytes_capped > 0);
    assert(size_bytes_capped <= MB_TO_BYTES(cap_mib));

    // 4. Verify "zram-size = ram * 2" (Aggressive scale)
    u64 size_bytes_aggressive = zram_size_bytes_expr_aggressive(mem_total_kib);
    assert(size_bytes_aggressive > 0);
    assert(size_bytes_aggressive == size_bytes_ram * 2);

    return 0;
}
