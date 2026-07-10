#!/bin/bash
#SBATCH --job-name=p_suche_s30
#SBATCH --account=l_iui_uelschen_pva_ws25
#SBATCH --output=results/search_s30_%j.out
#SBATCH --error=results/search_s30_%j.err
#SBATCH --time=24:00:00
#SBATCH --cpus-per-task=16
#SBATCH --mem=32G

set -euo pipefail

cd "${SLURM_SUBMIT_DIR}"

source scripts/hpc/setup_env.sh

TARGET_SIZE="${TARGET_SIZE:-30}"
THREADS="${SLURM_CPUS_PER_TASK:-16}"
JOB_ID="${SLURM_JOB_ID:-0}"
RUN_TS="$(date '+%Y-%m-%d %H:%M:%S')"

mkdir -p results build

echo "=== Search odd n until optimal size >= ${TARGET_SIZE} ==="
echo "host=$(hostname)  threads=${THREADS}  job=${JOB_ID}  date=$(date -Iseconds)"

make clean
make BUILD=release

known_lb() {
    case "$1" in
        3) echo 3 ;;
        5) echo 7 ;;
        7) echo 13 ;;
        9) echo 19 ;;
        11) echo 25 ;;
        13) echo 32 ;;
        15) echo 41 ;;
        17) echo 49 ;;
        19) echo 58 ;;
        *) echo 1 ;;
    esac
}

for n in 3 5 7 9 11 13 15 17 19; do
    lb="$(known_lb "${n}")"
    echo ""
    echo "======== n=${n} (lb=${lb}) ========"
    log="results/median_n${n}_size_compare.log"
    out="results/median_n${n}_size_first.txt"

    SECONDS=0
    ./build/p_suche --n "${n}" --opt size --compare --threads "${THREADS}" --first --lb "${lb}" \
        --out "${out}" 2>&1 | tee "${log}"
    wall_s="${SECONDS}"
    echo "wall_elapsed ${wall_s}s"

    if scripts/hpc/append_benchmark_csv.sh "${log}" "${n}" "${THREADS}" "${JOB_ID}" "${RUN_TS}"; then
        echo "CSV updated for n=${n}"
    else
        echo "WARN: could not append CSV for n=${n}" >&2
    fi

    opt="$(grep 'Optimal (size):' "${log}" | tail -1 | sed -E 's/.*Optimal \(size\): ([0-9]+).*/\1/')"
    seq_s="$(grep -E '^Sequentiell:' "${log}" | tail -1 | sed -E 's/.*Sequentiell:[[:space:]]+([0-9.]+)s.*/\1/')"
    par_s="$(grep -E '^Parallel:' "${log}" | tail -1 | sed -E 's/.*Parallel:[[:space:]]+([0-9.]+)s.*/\1/')"
    spd="$(grep -E '^Speedup S:' "${log}" | tail -1 | sed -E 's/.*Speedup S:[[:space:]]+([0-9.]+).*/\1/')"
    echo "RESULT n=${n} optimal_size=${opt:-unknown} sequential=${seq_s:-?}s parallel=${par_s:-?}s speedup=${spd:-?} wall=${wall_s}s"

    if [[ -n "${opt}" && "${opt}" -ge "${TARGET_SIZE}" ]]; then
        echo "=== TARGET REACHED: n=${n} optimal size=${opt} (>= ${TARGET_SIZE}) ==="
        exit 0
    fi
done

echo "=== Finished all n up to 19 without reaching size ${TARGET_SIZE} ==="
