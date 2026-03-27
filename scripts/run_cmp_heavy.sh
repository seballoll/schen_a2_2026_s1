#!/usr/bin/env bash
set -euo pipefail

# Usage:
#   ./scripts/run_cmp_heavy.sh [runner] [out_dir] [runs] [steps] [particles] [thread_counts_csv] [disable_smt]
# Example:
#   ./scripts/run_cmp_heavy.sh "build_root_test/Proyecto Individual/lab_runner" \
#       benchmark_artifacts/cmp_heavy 12 50 1200 "2,4,6,8,10,12,14,16,18" yes

RUNNER="${1:-build_root_test/Proyecto Individual/lab_runner}"
OUT_DIR="${2:-benchmark_artifacts/cmp_heavy}"
RUNS="${3:-12}"
STEPS="${4:-50}"
PARTICLES="${5:-1200}"
THREADS_CSV="${6:-2,4,6,8,10,12,14,16,18}"
DISABLE_SMT="${7:-no}"
SMT_CONTROL_PATH="/sys/devices/system/cpu/smt/control"
PREV_SMT_STATE=""

if [[ ! -x "$RUNNER" ]]; then
  echo "Runner not found or not executable: $RUNNER" >&2
  exit 1
fi

cleanup_smt() {
  if [[ "$DISABLE_SMT" == "yes" && -n "$PREV_SMT_STATE" ]]; then
    printf "%s\n" "$PREV_SMT_STATE" | sudo -n tee "$SMT_CONTROL_PATH" >/dev/null || true
  fi
}
trap cleanup_smt EXIT

if [[ "$DISABLE_SMT" == "yes" ]]; then
  if [[ ! -w "$SMT_CONTROL_PATH" ]]; then
    PREV_SMT_STATE="$(cat "$SMT_CONTROL_PATH")"
    printf "off\n" | sudo -n tee "$SMT_CONTROL_PATH" >/dev/null
  else
    PREV_SMT_STATE="$(cat "$SMT_CONTROL_PATH")"
    printf "off\n" > "$SMT_CONTROL_PATH"
  fi
fi

mkdir -p "$OUT_DIR"
RAW_CSV="$OUT_DIR/raw_cmp_runs.csv"
SUMMARY_CSV="$OUT_DIR/summary_cmp.csv"
PLOT_PNG="$OUT_DIR/cmp_heavy_trend.png"

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

echo "Running heavy CMP benchmark..."
echo "- runner: $RUNNER"
echo "- runs: $RUNS"
echo "- steps: $STEPS"
echo "- particles: $PARTICLES"
echo "- threads: $THREADS_CSV"
echo "- disable SMT: $DISABLE_SMT"

for ((i=0; i<RUNS; i++)); do
  output="$("$RUNNER" "$STEPS" "$PARTICLES" 1 sequential)"
  wall="$(printf "%s\n" "$output" | sed -n 's/^Total wall (ms):[[:space:]]*//p' | tail -1)"
  cycles="$(printf "%s\n" "$output" | sed -n 's/^Total cycles:[[:space:]]*//p' | tail -1)"
  printf "sequential,1,%s,%s,%s\n" "$i" "$wall" "$cycles" >> "$RAW_CSV"
done

IFS=',' read -r -a THREADS <<< "$THREADS_CSV"
for t in "${THREADS[@]}"; do
  t_trimmed="$(echo "$t" | xargs)"
  [[ -z "$t_trimmed" ]] && continue
  echo "  -> cmp_t$t_trimmed"
  for ((i=0; i<RUNS; i++)); do
    run_and_capture "cmp" "$t_trimmed" "$i"
  done
done

{
  echo "strategy,threads,runs,wall_mean_ms,wall_std_ms,cycles_mean,cycles_std"
  awk -F, '
NR==1 { next }
{
  key=$1","$2
  n[key]++
  sum_wall[key]+=$4
  sumsq_wall[key]+=$4*$4
  sum_cycles[key]+=$5
  sumsq_cycles[key]+=$5*$5
}
END {
  for (k in n) {
    mean_w=sum_wall[k]/n[k]
    mean_c=sum_cycles[k]/n[k]
    if (n[k] > 1) {
      var_w=(sumsq_wall[k] - (sum_wall[k]*sum_wall[k]/n[k]))/(n[k]-1)
      var_c=(sumsq_cycles[k] - (sum_cycles[k]*sum_cycles[k]/n[k]))/(n[k]-1)
      if (var_w < 0) var_w=0
      if (var_c < 0) var_c=0
      std_w=sqrt(var_w)
      std_c=sqrt(var_c)
    } else {
      std_w=0
      std_c=0
    }
    split(k, parts, ",")
    printf "%s,%s,%d,%.8f,%.8f,%.8f,%.8f\n", parts[1], parts[2], n[k], mean_w, std_w, mean_c, std_c
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

threads = []
wall = []
seq_wall = None

with summary_path.open("r", encoding="utf-8") as f:
    reader = csv.DictReader(f)
    for row in reader:
        strategy = row["strategy"].strip()
        t = int(row["threads"])
        w = float(row["wall_mean_ms"])
        if strategy == "sequential":
            seq_wall = w
        elif strategy == "cmp":
            threads.append(t)
            wall.append(w)

if not threads:
    raise SystemExit("No CMP data found in summary CSV.")

pairs = sorted(zip(threads, wall), key=lambda p: p[0])
threads = [p[0] for p in pairs]
wall = [p[1] for p in pairs]

plt.figure(figsize=(10, 6))
plt.plot(threads, wall, marker="s", linewidth=2.0, label="CMP")
if seq_wall is not None:
    plt.plot(
        threads,
        [seq_wall for _ in threads],
        linestyle="--",
        color="black",
        linewidth=1.3,
        label="Sequential baseline",
    )
plt.xticks(threads, [str(t) for t in threads])
plt.xlabel("Thread count")
plt.ylabel("Mean wall time (ms)")
plt.title("Heavy CMP execution trend")
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
