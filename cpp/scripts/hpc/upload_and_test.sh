#!/bin/bash
# Sync project to HPC (~/PVA) and submit smoke test.
# Usage: ./cpp/scripts/hpc/upload_and_test.sh

set -euo pipefail

REMOTE="${HPC_USER:-abennani}@${HPC_HOST:-hpc.hs-osnabrueck.de}"
REMOTE_DIR="${HPC_DIR:-~/PVA}"
SSH_OPTS=(-o ConnectTimeout=15)
if [[ -f "$HOME/.ssh/hiper4all/id_ed25519_HCP" ]]; then
    SSH_OPTS+=(-i "$HOME/.ssh/hiper4all/id_ed25519_HCP")
fi

ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
echo "Syncing $ROOT -> ${REMOTE}:${REMOTE_DIR}"

rsync -avz --delete \
    --exclude '.git' \
    --exclude 'cpp/build' \
    -e "ssh ${SSH_OPTS[*]}" \
    "$ROOT/" "${REMOTE}:${REMOTE_DIR}/"

echo "Submitting smoke test..."
ssh "${SSH_OPTS[@]}" "$REMOTE" "cd ${REMOTE_DIR}/cpp && chmod +x scripts/hpc/*.sh && sbatch scripts/hpc/smoke_test.slurm"
echo "Done. Check: ssh hpc 'squeue -u \$USER'"
