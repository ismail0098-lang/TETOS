
---

## Verifications

| Stage | Subsystem | Target Functions | Properties | Status | Core Mathematical Focus |
| :---: | :--- | :--- | :---: | :---: | :--- |
| **1** | **POC Round-Robin** | `poc_select_rr_improved`, `poc_select_rr` | 21 | **SUCCESS** | Correctness of Improved vs. Legacy selection, Golden Ratio Scrambling bounds. |
| **2** | **Bitmask Packing** | `poc_flags_to_u64` | 31 | **SUCCESS** | Equivalence of word-level multiply-and-shift optimization (`0x0102040810204080ULL`). |
| **3** | **Cluster Search** | `poc_cluster_search` | 23 | **SUCCESS** | Topology-local idle CPU masking and intersection validity. |
| **4** | **BPF land Math** | `task_dl`, `task_slice` | 67 | **SUCCESS** | Division-by-zero protection (e.g. `weight` scaling) and monotonic deadline bounds. |
| **5** | **BORE Math** | `log2p1_u64_u32fp`, `calc_burst_penalty`, `binary_smooth` | 18 | **SUCCESS** | Undefined shift distance, clz/De Bruijn index safety, and smoothing bounds. |

---

##  Harness Details & Specifications

###  1. Bounded Round-Robin CPU Selection (`harness_rr.c`)
Proves the safety of the Piece-Of-Cake (POC) idle CPU selection algorithm.
* **Property Checked:** Guaranteed to select an idle CPU from the mask that is within the range `[base, base + 63]` without signed overflow.
* **Key Scrambling Multiplier:** `0x9E3779B9U` (Golden Ratio).

### 🗜️ 2. Flag-to-Bitmask Packing (`harness_packing.c`)
Validates the highly optimized byte-array-to-bitmask compressor.
* **Property Checked:** Proves that the word-level parallel packing expression:
  $$\text{packed} = \sum_{i=0}^{7} \left( \frac{(w_i \ \& \ 0x0101010101010101) \times 0x0102040810204080}{2^{56}} \right) \ll (i \times 8)$$
  is logically equivalent to a sequential loop over all 64 elements for all $2^{64}$ symbolic combinations.

###  3. CPU Cluster Search (`harness_cluster.c`)
Proves correctness of Cache-Cluster-Local CPU searching.
* **Property Checked:** Ensures the selector returns a CPU that is both globally idle and is a member of the target CPU's L2/L3 cache cluster mask.

### ⏱ 4. `scx_bpfland` Deadline & Slice Math (`harness_bpfland_dl.c`)
Verifies execution deadline math for BPF-based scheduling.
* **Property Checked:** Proves that when the scheduler scales virtual runtimes (`scale_by_task_weight_inverse`), it is protected from division-by-zero panics and monotonic violations (`dl >= dsq_vtime`).

###  5. BORE Scheduler Math Logic (`harness_bore_math.c`)
Verifies BORE's custom fixed-point logarithmic priority scaling.
* **Property Checked:** Proves that the fixed-point logarithm `log2p1_u64_u32fp` is safe from undefined shift distances, and `binary_smooth` bounds outputs correctly without signed integer overflows.

---

 Tech Stack & Verification Setup

* **Front-end:** CBMC (Bounded Model Checker)
* **Back-end SMT Solver:** Z3 Theorem Prover (Booled ASS version)
* **Target Language:** GNU C / Linux Kernel BPF
* **Unwind Bounds:** 65 (covers all 64-bit arrays/bitmasks fully)
