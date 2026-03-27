#!/usr/bin/env bash
set -euo pipefail

# Usage:
#   ./scripts/run_smt_heavy.sh [runner] [out_dir] [runs] [steps] [particles] [thread_counts_csv]
# Example:
#   ./scripts/run_smt_heavy.sh "build_root_test/Proyecto Individual/lab_runner" \
#       benchmark_artifacts/smt_heavy 12 50 1200 "2,4,6,8,10,12,14,16,18"

RUNNER="${1:-build_root_test/Proyecto Individual/lab_runner}"
OUT_DIR="${2:-benchmark_artifacts/smt_heavy}"
RUNS="${3:-12}"
STEPS="${4:-50}"
PARTICLES="${5:-1200}"
THREADS_CSV="${6:-2,4,6,8,10,12,14,16,18}"

if [[ ! -x "$RUNNER" ]]; then
  echo "Runner not found or not executable: $RUNNER" >&2
  exit 1
fi

mkdir -p "$OUT_DIR"
RAW_CSV="$OUT_DIR/raw_smt_runs.csv"
SUMMARY_CSV="$OUT_DIR/summary_smt.csv"
PLOT_PNG="$OUT_DIR/smt_heavy_trend.png"

printf "strategy,threads,run_index,wall_ms,total_cycles\n" > "$RAW_CSV"

run_and_capture() {
  local strategy="$1"
  local threads="$2"
  local run_idx="$3"
  local output
  local wall
  local cycles

  output="$("$RUNNER" "$STEPS" "$PARTICLES" "$threads" "$strategy")"

  wall="$(printf "%s\n" "$output" | sed -n 's/^Total wall (ms):[[:space:]]*//p' | tail -1)"
  cycles="$(printf "%s\n" "$output" | sed -n 's/^Total cycles:[[:space:]]*//p' | tail -1)"

  if [[ -z "$wall" || -z "$cycles" ]]; then
    echo "Could not parse runner output for $strategy t=$threads run=$run_idx" >&2
    echo "$output" >&2
    exit 1
  fi

  printf "%s,%s,%s,%s,%s\n" "$strategy" "$threads" "$run_idx" "$wall" "$cycles" >> "$RAW_CSV"
}

echo "Running heavy SMT benchmark..."
echo "- runner: $RUNNER"
echo "- runs: $RUNS"
echo "- steps: $STEPS"
echo "- particles: $PARTICLES"
echo "- threads: $THREADS_CSV"

for ((i=0; i<RUNS; i++)); do
  run_and_capture "sequential" "1" "$i"
done

IFS=',' read -r -a THREADS <<< "$THREADS_CSV"
for t in "${THREADS[@]}"; do
  t_trimmed="$(echo "$t" | xargs)"
  [[ -z "$t_trimmed" ]] && continue
  echo "  -> smt_t$t_trimmed"
  for ((i=0; i<RUNS; i++)); do
    run_and_capture "smt" "$t_trimmed" "$i"
  done
done

{
  echo "strategy,threads,runs,wall_mean_ms,wall_std_ms"
  awk -F, '
NR==1 { next }
{
  key=$1","$2
  n[key]++
  sum_wall[key]+=$4
  sumsq_wall[key]+=$4*$4
}
END {
  for (k in n) {
    mean=sum_wall[k]/n[k]
    if (n[k] > 1) {
      var=(sumsq_wall[k] - (sum_wall[k]*sum_wall[k]/n[k]))/(n[k]-1)
      if (var < 0) var=0
      std=sqrt(var)
    } else {
      std=0
    }
    split(k, parts, ",")
    printf "%s,%s,%d,%.8f,%.8f\n", parts[1], parts[2], n[k], mean, std
  }
}
' "$RAW_CSV" | sort -t, -k1,1 -k2,2n
} > "$SUMMARY_CSV"

python3 - "$SUMMARY_CSV" "$PLOT_PNG" <<'PY'
import csv
import sys
from pathlib import Path

summary_path = Path(sys.argv[1])
plot_path = Path(sys.argv[2])

try:
    import matplotlib.pyplot as plt
except Exception as exc:
    print(f"Plot skipped: matplotlib not available ({exc})")
    raise SystemExit(0)

smt_threads = []
smt_mean = []
seq_mean = None

with summary_path.open("r", encoding="utf-8") as f:
    reader = csv.DictReader(f)
    for row in reader:
        strategy = row["strategy"].strip()
        threads = int(row["threads"])
        mean_ms = float(row["wall_mean_ms"])
        if strategy == "sequential":
            seq_mean = mean_ms
        elif strategy == "smt":
            smt_threads.append(threads)
            smt_mean.append(mean_ms)

if not smt_threads:
    raise SystemExit("No SMT data found in summary CSV.")

pairs = sorted(zip(smt_threads, smt_mean), key=lambda p: p[0])
smt_threads = [p[0] for p in pairs]
smt_mean = [p[1] for p in pairs]

plt.figure(figsize=(10, 6))
plt.plot(smt_threads, smt_mean, marker="o", linewidth=2.0, label="SMT")
if seq_mean is not None:
    plt.plot(
        smt_threads,
        [seq_mean for _ in smt_threads],
        linestyle="--",
        color="black",
        linewidth=1.3,
        label="Sequential baseline",
    )
plt.xticks(smt_threads, [str(t) for t in smt_threads])
plt.xlabel("Thread count")
plt.ylabel("Mean wall time (ms)")
plt.title("Heavy SMT execution trend")
plt.grid(axis="y", alpha=0.3)
plt.legend()
plt.tight_layout()
plt.savefig(plot_path, dpi=160)
plt.close()

print(f"Plot generated: {plot_path}")
PY

echo "Done."
echo "- Raw CSV: $RAW_CSV"
echo "- Summary: $SUMMARY_CSV"
echo "- Plot: $PLOT_PNG"
