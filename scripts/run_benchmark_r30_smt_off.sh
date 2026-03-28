#!/usr/bin/env bash
set -euo pipefail

# Run the closing 30-run benchmark campaigns while forcing SMT OFF during CMP phases.
# This script must run as root because it writes /sys/devices/system/cpu/smt/control.

if [[ "${EUID}" -ne 0 ]]; then
  echo "Error: this script must run as root (use: sudo ./scripts/run_benchmark_r30_smt_off.sh)." >&2
  exit 1
fi

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
RUNNER="${1:-./Proyecto Individual/build/lab_runner}"
OUT_BASE="${2:-benchmark_artifacts}"
STEPS="${3:-50}"
PARTICLES="${4:-1200}"
THREAD_COUNTS="${5:-2,4,6,8,10,12,14,16,18}"
FGMT_QUANTUM="${6:-12}"
SEED_START="${7:-1}"

cd "${ROOT_DIR}"

python3 scripts/run_benchmark.py \
  --runner "${RUNNER}" \
  --out-dir "${OUT_BASE}/closing_fixed_r30_smt_off" \
  --runs 30 \
  --steps "${STEPS}" \
  --particles "${PARTICLES}" \
  --thread-counts "${THREAD_COUNTS}" \
  --fgmt-quantum "${FGMT_QUANTUM}" \
  --seed-start "${SEED_START}" \
  --seed-mode fixed \
  --cmp-disable-smt

python3 scripts/run_benchmark.py \
  --runner "${RUNNER}" \
  --out-dir "${OUT_BASE}/closing_per_round_r30_smt_off" \
  --runs 30 \
  --steps "${STEPS}" \
  --particles "${PARTICLES}" \
  --thread-counts "${THREAD_COUNTS}" \
  --fgmt-quantum "${FGMT_QUANTUM}" \
  --seed-start "${SEED_START}" \
  --seed-mode per-round \
  --cmp-disable-smt

echo "Done. Outputs:"
echo "- ${OUT_BASE}/closing_fixed_r30_smt_off"
echo "- ${OUT_BASE}/closing_per_round_r30_smt_off"
