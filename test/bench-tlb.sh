#!/bin/sh

num_reps=9
event=dTLB-store-misses
exec_prefix="numactl --physcpubind=2"

# first run: 256k threshold makes all allocation land in the pool
tlb_mr_hp="$(HPPA_ALLOC_THRESHOLD=$((256 * 1024)) perf stat -e$event -x: -r$num_reps $exec_prefix ./stress 2>&1 > /dev/null | cut -f1 -d:)"

echo "TLB misses (huge pages): $tlb_mr_hp"

# second run: 2GB threshold no allocations land in the pool but goes via malloc internally
tlb_mr_malloc="$(HPPA_ALLOC_THRESHOLD=$((2 * 1024 * 1024 * 1024)) perf stat -e$event -x: -r$num_reps $exec_prefix ./stress 2>&1 > /dev/null | cut -f1 -d:)"

echo "TLB misses (malloc): $tlb_mr_malloc"

echo -n 'ratio (%): '
echo "scale = 3; $tlb_mr_hp / $tlb_mr_malloc * 100" | bc -l
