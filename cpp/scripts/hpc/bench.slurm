#!/bin/bash
#SBATCH --job-name=p_suche_bench
#SBATCH --account=l_iui_uelschen_pva_ws25
#SBATCH --output=p_suche_bench_%j.out
#SBATCH --error=p_suche_bench_%j.err
#SBATCH --time=02:00:00
#SBATCH --cpus-per-task=8
#SBATCH --mem=16G

set -euo pipefail

cd "${SLURM_SUBMIT_DIR}"

source scripts/hpc/setup_env.sh

THREADS="${SLURM_CPUS_PER_TASK:-8}"
JOB_ID="${SLURM_JOB_ID:-0}"
RUN_TS="$(date '+%Y-%m-%d %H:%M:%S')"

mkdir -p results build

echo "=== p_suche HPC benchmark (sequential vs TBB --compare) ==="
echo "host=$(hostname)  threads=${THREADS}  job=${JOB_ID}  date=$(date -Iseconds)"

make clean
make BUILD=release

for n in 5 7; do
    lb="$(case "${n}" in 5) echo 7 ;; 7) echo 13 ;; esac)"
    log="results/hpc_n${n}_compare.log"
    echo "--- n=${n} size: sequential vs TBB ---"
    ./build/p_suche --n "${n}" --opt size --compare --first --threads "${THREADS}" --lb "${lb}" \
        --out "results/hpc_n${n}_compare.txt" 2>&1 | tee "${log}"
    scripts/hpc/append_benchmark_csv.sh "${log}" "${n}" "${THREADS}" "${JOB_ID}" "${RUN_TS}" || true
done

echo "=== HPC benchmark done ==="
