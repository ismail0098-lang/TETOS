#!/bin/bash
DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cp "/home/yumin/NVME files/YSU-engine-main/YSU-engine-main/src/Y_lang/z3/build/z3" "$DIR/z3"
chmod +x "$DIR/z3"
echo "Z3 binary copied successfully to verification folder."
