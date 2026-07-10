#!/bin/bash
# Load compiler / TBB on HS Osnabrück HPC.

module purge 2>/dev/null || true

if module avail gcc 2>&1 | grep -q gcc; then
    module load gcc
fi

if module avail tbb 2>&1 | grep -q tbb; then
    module load tbb
    export TBBROOT="${TBBROOT:-$TBB_ROOT}"
fi

# Intel oneAPI TBB (no Lmod module on hiper4all)
if [[ -z "${TBBROOT:-}" ]]; then
    for candidate in \
        /opt/intel/oneapi/tbb/latest \
        /opt/intel/oneapi/2025.3 \
        /opt/intel/oneapi/2024.2; do
        if [[ -f "${candidate}/include/tbb/concurrent_vector.h" ]]; then
            export TBBROOT="${candidate}"
            break
        fi
    done
fi

if [[ -n "${TBBROOT:-}" ]]; then
    export LD_LIBRARY_PATH="${TBBROOT}/lib/intel64:${TBBROOT}/lib:${LD_LIBRARY_PATH:-}"
fi

export CXX="${CXX:-g++}"
echo "CXX=$CXX ($($CXX --version | head -1))"
if [[ -n "${TBBROOT:-}" ]]; then
    echo "TBBROOT=$TBBROOT"
fi
