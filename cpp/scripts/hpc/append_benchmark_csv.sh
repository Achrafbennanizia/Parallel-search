#!/bin/bash
# Parse p_suche --compare output and append rows to results/benchmark_results.csv
# Usage: append_benchmark_csv.sh <logfile> <n> <threads> [job_id] [timestamp]

set -euo pipefail

LOG="${1:?log file required}"
N="${2:?n required}"
THREADS="${3:?threads required}"
JOB_ID="${4:-}"
TS="${5:-$(date '+%Y-%m-%d %H:%M:%S')}"
CSV="${BENCHMARK_CSV:-results/benchmark_results.csv}"
TAG="${BENCHMARK_TAG:-hpc}"

if [[ ! -f "${LOG}" ]]; then
    echo "append_benchmark_csv: missing log ${LOG}" >&2
    exit 1
fi

seq_s="$(grep -E '^Sequentiell:' "${LOG}" | tail -1 | sed -E 's/.*Sequentiell:[[:space:]]+([0-9.]+)s.*/\1/')"
par_s="$(grep -E '^Parallel:' "${LOG}" | tail -1 | sed -E 's/.*Parallel:[[:space:]]+([0-9.]+)s.*/\1/')"
speedup="$(grep -E '^Speedup S:' "${LOG}" | tail -1 | sed -E 's/.*Speedup S:[[:space:]]+([0-9.]+).*/\1/')"
opt="$(grep -E '^Optimal \(size\):' "${LOG}" | tail -1 | sed -E 's/.*Optimal \(size\): ([0-9]+).*/\1/')"
nodes="$(grep -E '^Parallel:' "${LOG}" | tail -1 | sed -E 's/.*Knoten=([0-9]+).*/\1/')"

if [[ -z "${seq_s}" || -z "${par_s}" ]]; then
    echo "append_benchmark_csv: no compare block in ${LOG}" >&2
    exit 1
fi

seq_ms="$(awk -v s="${seq_s}" 'BEGIN { printf "%.6f", s * 1000 }')"
par_ms="$(awk -v s="${par_s}" 'BEGIN { printf "%.6f", s * 1000 }')"
cmp_ms="$(awk -v a="${seq_ms}" -v b="${par_ms}" 'BEGIN { printf "%.6f", a + b }')"
detail="nodes=${nodes:-?} --lb --compare"
if [[ -n "${JOB_ID}" ]]; then
    detail="HPC job=${JOB_ID} ${detail}"
fi

mkdir -p "$(dirname "${CSV}")"
if [[ ! -s "${CSV}" ]]; then
    echo "timestamp,build,threads,benchmark_name,duration_ms,speedup,networks_found,optimum,detail" > "${CSV}"
fi

append_row() {
    local name="$1" thr="$2" ms="$3" spd="${4:-}" optv="${5:-}" det="$6"
    {
        printf '%s,release,%s,%s,%s,' "${TS}" "${thr}" "${name}" "${ms}"
        if [[ -n "${spd}" ]]; then printf '%s' "${spd}"; fi
        printf ',1,'
        if [[ -n "${optv}" ]]; then printf '%s' "${optv}"; fi
        printf ',%s\n' "${det}"
    } >> "${CSV}"
}

append_row "search n=${N} size sequential [${TAG}]" 1 "${seq_ms}" "" "${opt}" "${detail}"
append_row "search n=${N} size threads=${THREADS} [${TAG}]" "${THREADS}" "${par_ms}" "${speedup}" "${opt}" "speedup vs sequential"
append_row "compare n=${N} size sequential vs tbb [${TAG}]" "${THREADS}" "${cmp_ms}" "${speedup}" "${opt}" "one run: --compare"

echo "append_benchmark_csv: n=${N} seq=${seq_ms}ms par=${par_ms}ms speedup=${speedup} -> ${CSV}"
