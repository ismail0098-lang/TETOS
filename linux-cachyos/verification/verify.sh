#!/bin/bash

# Exit on any error
set -e

# Base path
DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

echo "=== Formally Verifying CachyOS Scheduler Logic & Settings using CBMC + Z3 ==="

# Check if CBMC is available
if ! command -v cbmc &> /dev/null; then
    echo "ERROR: cbmc is not installed or not in PATH."
    echo "Please run: paru -S cbmc"
    exit 1
fi

# Add YSU Engine's built Z3 binary to PATH, and fallback to local folder
export PATH="/home/yumin/NVME files/YSU-engine-main/YSU-engine-main/src/Y_lang/z3/build:$DIR:$PATH"

# Check if Z3 is available
if ! command -v z3 &> /dev/null; then
    echo "ERROR: z3 is not found in PATH, YSU Engine build directory, or verification folder."
    echo "Please copy the z3 binary to the verification folder or run copy_z3.sh on the host."
    exit 1
fi

echo "[1/8] Verifying Bounded Round-Robin CPU Selection (harness_rr.c)..."
cbmc "$DIR/harness_rr.c" \
    --z3 \
    --pointer-check \
    --bounds-check \
    --div-by-zero-check \
    --signed-overflow-check \
    --unwind 65 \
    --trace

echo "SUCCESS: Bounded Round-Robin CPU selection verified safe and correct."
echo "---------------------------------------------------------------------"

echo "[2/8] Verifying Flag-to-Bitmask Packing (harness_packing.c)..."
cbmc "$DIR/harness_packing.c" \
    --z3 \
    --pointer-check \
    --bounds-check \
    --div-by-zero-check \
    --signed-overflow-check \
    --unwind 65 \
    --trace

echo "SUCCESS: Flag-to-Bitmask packing verified safe and correct."
echo "---------------------------------------------------------------------"

echo "[3/8] Verifying CPU Cluster Search (harness_cluster.c)..."
cbmc "$DIR/harness_cluster.c" \
    --z3 \
    --pointer-check \
    --bounds-check \
    --div-by-zero-check \
    --signed-overflow-check \
    --unwind 65 \
    --trace

echo "SUCCESS: CPU cluster search verified safe and correct."
echo "---------------------------------------------------------------------"

echo "[4/8] Verifying scx_bpfland Task Deadline & Slice Math (harness_bpfland_dl.c)..."
cbmc "$DIR/harness_bpfland_dl.c" \
    --z3 \
    --pointer-check \
    --bounds-check \
    --div-by-zero-check \
    --signed-overflow-check \
    --unwind 65 \
    --trace

echo "SUCCESS: scx_bpfland task deadline and slice math verified safe and correct."
echo "---------------------------------------------------------------------"

echo "[5/8] Verifying BORE Scheduler Math Logic (harness_bore_math.c)..."
cbmc "$DIR/harness_bore_math.c" \
    --z3 \
    --pointer-check \
    --bounds-check \
    --div-by-zero-check \
    --signed-overflow-check \
    --unwind 65 \
    --trace

echo "SUCCESS: BORE scheduler math logic verified safe and correct."
echo "---------------------------------------------------------------------"

echo "[6/8] Verifying EEVDF Core Scheduling Math (harness_eevdf_math.c)..."
cbmc "$DIR/harness_eevdf_math.c" \
    --z3 \
    --pointer-check \
    --bounds-check \
    --div-by-zero-check \
    --signed-overflow-check \
    --unwind 65 \
    --trace

echo "SUCCESS: EEVDF core scheduling math verified safe and correct."
echo "---------------------------------------------------------------------"

echo "[7/8] Verifying Sysctl Memory & I/O Interaction Model (harness_sysctl_mem.c)..."
cbmc "$DIR/harness_sysctl_mem.c" \
    --z3 \
    --pointer-check \
    --bounds-check \
    --div-by-zero-check \
    --signed-overflow-check \
    --unwind 10 \
    --trace

echo "SUCCESS: Sysctl memory reclaim & OOM interaction model verified safe."
echo "---------------------------------------------------------------------"

echo "[8/8] Verifying ZRAM Generator Allocation Math (harness_zram_math.c)..."
cbmc "$DIR/harness_zram_math.c" \
    --z3 \
    --pointer-check \
    --bounds-check \
    --div-by-zero-check \
    --signed-overflow-check \
    --unwind 10 \
    --trace

echo "SUCCESS: ZRAM generator memory calculation verified correct."
echo "====================================================================="
