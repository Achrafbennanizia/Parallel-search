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

export CXX="${CXX:-g++}"
echo "CXX=$CXX ($($CXX --version | head -1))"
