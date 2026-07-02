#!/bin/bash
#SBATCH --job-name=p_suche
#SBATCH --account=l_iui_uelschen_pva_ws25
#SBATCH --output=p_suche_%j.out
#SBATCH --error=p_suche_%j.err
#SBATCH --time=02:00:00
#SBATCH --cpus-per-task=8
#SBATCH --mem=16G

set -euo pipefail

cd "${SLURM_SUBMIT_DIR}"

source scripts/hpc/setup_env.sh

THREADS="${SLURM_CPUS_PER_TASK:-8}"

make clean
make OPENMP=1 BUILD=release

mkdir -p results build
/usr/bin/time -f "elapsed %e s" ./build/p_suche --n 7 --opt size --threads "${THREADS}" \
  --out results/median_n7_size.txt
