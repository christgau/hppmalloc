#!/bin/sh

num_reps=9
event=dTLB-store-misses
exec_prefix="numactl --physcpubind=2"

export HPPA_SIZE_ANON=$(($(cat /sys/kernel/mm/hugepages/hugepages-1048576kB/free_hugepages)*1024*1024*1024))
export HPPA_ALLOC_THRESHOLD=$((512 * 1024))

# first run: 256k threshold makes all allocation land in the pool
#tlb_nm_hp="$(HPPA_ALLOC_THRESHOLD=$((256 * 1024)) perf stat -e$event -x: -r$num_reps $exec_prefix ./stress 2>&1 > /dev/null | cut -f1 -d:)"
tlb_nm_hp="$(HPPA_INITIAL_STRATEGY=2 perf stat -e$event -x: -r$num_reps $exec_prefix ./stress 2>&1 > /dev/null | cut -f1 -d:)"

echo "TLB misses (huge pages): $tlb_nm_hp"

# second run: 2GB threshold no allocations land in the pool but goes via malloc internally
#tlb_nm_malloc="$(HPPA_ALLOC_THRESHOLD=$((2 * 1024 * 1024 * 1024)) perf stat -e$event -x: -r$num_reps $exec_prefix ./stress 2>&1 > /dev/null | cut -f1 -d:)"
tlb_nm_malloc="$(HPPA_INITIAL_STRATEGY=1 perf stat -e$event -x: -r$num_reps $exec_prefix ./stress 2>&1 > /dev/null | cut -f1 -d:)"

echo "TLB misses (malloc): $tlb_nm_malloc"

echo -n 'ratio (%): '
echo "scale = 3; $tlb_nm_hp / $tlb_nm_malloc * 100" | bc -l
