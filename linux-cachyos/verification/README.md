# TETOS — CachyOS Scheduler and Settings Verification

This repository formally verifies the safety and correctness of the CachyOS kernel schedulers and system parameters using **CBMC** and **Z3**.

## What we verified

**1. Round-Robin CPU Selection** (`harness_rr.c`)
Proves that idle CPU selection is always within CPU count limits and handles Golden Ratio scrambling safely.

**2. Bitmask Packing** (`harness_packing.c`)
Proves that the multiply-and-shift optimized byte-to-bitmask compressor is logically identical to a loop check.

**3. Cluster Search** (`harness_cluster.c`)
Proves topology-local idle CPU masking stays inside L2/L3 cache cluster boundaries.

**4. BPF Land Deadline Math** (`harness_bpfland_dl.c`)
Proves the virtual deadline calculations in the `scx_bpfland` scheduler never divide by zero or go backwards.

**5. BORE Burst Penalty Math** (`harness_bore_math.c`)
Proves the priority scaling functions and burst penalty smoothing are safe from overflows.

**6. EEVDF Core Scheduling Math** (`harness_eevdf_math.c`)
Proves EEVDF weighted runtime averages and wrap-around sequence number comparisons are safe from divisions by zero and overflow.

**7. Memory Pressure & OOM interaction** (`harness_sysctl_mem.c`)
Proves that the interaction of `vm.swappiness=100`, `vm.vfs_cache_pressure=50`, and user namespace limits under maximum RAM pressure will not cause OOM lockup or priority inversion.

**8. ZRAM Sizing Math** (`harness_zram_math.c`)
Proves the zram-size scaling math converts physical memory to bytes safely across huge RAM capacities (up to 16TB) without overflows or integer truncation.

## How to run it

```bash
cd linux-cachyos/verification
./verify.sh
```
