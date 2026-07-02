#!/bin/bash
#SBATCH --job-name=p_suche_smoke
#SBATCH --account=l_iui_uelschen_pva_ws25
#SBATCH --output=p_suche_smoke_%j.out
#SBATCH --error=p_suche_smoke_%j.err
#SBATCH --time=00:15:00
#SBATCH --cpus-per-task=2
#SBATCH --mem=4G

set -euo pipefail

cd "${SLURM_SUBMIT_DIR}"

source scripts/hpc/setup_env.sh

echo "=== p_suche HPC smoke test (TBB) ==="
make clean
make BUILD=release
make test BUILD=release

echo "=== smoke test OK ==="
