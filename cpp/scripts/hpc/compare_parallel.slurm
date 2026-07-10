#!/bin/bash
#SBATCH --job-name=p_suche_cmp
#SBATCH --account=l_iui_uelschen_pva_ws25
#SBATCH --output=results/compare_%j.out
#SBATCH --error=results/compare_%j.err
#SBATCH --time=04:00:00
#SBATCH --cpus-per-task=16
#SBATCH --mem=32G

set -euo pipefail

cd "${SLURM_SUBMIT_DIR}"

source scripts/hpc/setup_env.sh

THREADS="${SLURM_CPUS_PER_TASK:-16}"
JOB_ID="${SLURM_JOB_ID:-0}"
RUN_TS="$(date '+%Y-%m-%d %H:%M:%S')"

mkdir -p results build

echo "=== Sequential vs TBB compare (HPC) ==="
echo "host=$(hostname)  threads=${THREADS}  job=${JOB_ID}  date=$(date -Iseconds)"

make clean
make BUILD=release

run_compare() {
    local n="$1"
    local lb="${2:-}"
    local tag="$3"
    local log="results/compare_n${n}_${tag}.log"
    local out="results/compare_n${n}_${tag}.txt"
    local extra=()
    if [[ -n "${lb}" ]]; then
        extra=(--lb "${lb}")
    fi

    echo ""
    echo "======== n=${n} ${tag} ========"
    SECONDS=0
    ./build/p_suche --n "${n}" --opt size --compare --first --threads "${THREADS}" \
        "${extra[@]}" --out "${out}" 2>&1 | tee "${log}"
    echo "wall_elapsed ${SECONDS}s"

    BENCHMARK_TAG="hpc-${tag}" scripts/hpc/append_benchmark_csv.sh \
        "${log}" "${n}" "${THREADS}" "${JOB_ID}" "${RUN_TS}"

    grep -E '^(Sequentiell|Parallel|Speedup|Optimal)' "${log}" | tail -4
}

# n=5: full IDDFS from budget 1 — enough work for meaningful parallel speedup
run_compare 5 "" "full"

# n=7: proven lower bound only — sequential still heavy, parallel fast
run_compare 7 13 "lb13"

echo ""
echo "=== Compare benchmark done ==="
