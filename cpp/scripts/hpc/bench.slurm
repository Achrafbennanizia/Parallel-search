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
mkdir -p results build

echo "=== p_suche HPC benchmark (TBB --compare) ==="
echo "host=$(hostname)  threads=${THREADS}  date=$(date -Iseconds)"

make clean
make BUILD=release

echo "--- n=5 size: sequential vs TBB in one run ---"
./build/p_suche --n 5 --opt size --compare --first --threads "${THREADS}" \
  --out results/hpc_n5_compare.txt | tee results/hpc_n5_compare.log

echo "--- n=7 size: sequential vs TBB in one run ---"
./build/p_suche --n 7 --opt size --compare --first --threads "${THREADS}" \
  --out results/hpc_n7_compare.txt | tee results/hpc_n7_compare.log

echo "=== HPC benchmark done ==="
