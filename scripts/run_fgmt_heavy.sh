#!/usr/bin/env bash
set -euo pipefail

# Usage:
#   ./scripts/run_fgmt_heavy.sh [runner] [out_dir] [runs] [steps] [particles] [thread_counts_csv] [quantum]
# Example:
#   ./scripts/run_fgmt_heavy.sh "build_root_test/Proyecto Individual/lab_runner" \
#       benchmark_artifacts/fgmt_heavy 12 50 1200 "2,4,6,8,10,12,14,16,18" 12

RUNNER="${1:-build_root_test/Proyecto Individual/lab_runner}"
OUT_DIR="${2:-benchmark_artifacts/fgmt_heavy}"
RUNS="${3:-12}"
STEPS="${4:-50}"
PARTICLES="${5:-1200}"
THREADS_CSV="${6:-2,4,6,8,10,12,14,16,18}"
QUANTUM="${7:-12}"

if [[ ! -x "$RUNNER" ]]; then
  echo "Runner not found or not executable: $RUNNER" >&2
  exit 1
fi

mkdir -p "$OUT_DIR"
RAW_CSV="$OUT_DIR/raw_fgmt_runs.csv"
SUMMARY_CSV="$OUT_DIR/summary_fgmt.csv"
PLOT_PNG="$OUT_DIR/fgmt_heavy_trend.png"

printf "strategy,threads,run_index,wall_ms,total_cycles\n" > "$RAW_CSV"

run_and_capture() {
  local strategy="$1"
  local threads="$2"
  local run_idx="$3"
  local output
  local wall
  local cycles

  output="$("$RUNNER" "$STEPS" "$PARTICLES" "$threads" "$strategy" "$QUANTUM")"

  wall="$(printf "%s\n" "$output" | sed -n 's/^Total wall (ms):[[:space:]]*//p' | tail -1)"
  cycles="$(printf "%s\n" "$output" | sed -n 's/^Total cycles:[[:space:]]*//p' | tail -1)"

  if [[ -z "$wall" || -z "$cycles" ]]; then
    echo "Could not parse runner output for $strategy t=$threads run=$run_idx" >&2
    echo "$output" >&2
    exit 1
  fi

  printf "%s,%s,%s,%s,%s\n" "$strategy" "$threads" "$run_idx" "$wall" "$cycles" >> "$RAW_CSV"
}

echo "Running heavy FGMT benchmark..."
echo "- runner: $RUNNER"
echo "- runs: $RUNS"
echo "- steps: $STEPS"
echo "- particles: $PARTICLES"
echo "- threads: $THREADS_CSV"
echo "- quantum: $QUANTUM"

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
  echo "  -> fgmt_t$t_trimmed"
  for ((i=0; i<RUNS; i++)); do
    run_and_capture "fgmt" "$t_trimmed" "$i"
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
cycles = []
seq_cycles = None

with summary_path.open("r", encoding="utf-8") as f:
    reader = csv.DictReader(f)
    for row in reader:
        strategy = row["strategy"].strip()
        t = int(row["threads"])
        c = float(row["cycles_mean"])
        if strategy == "sequential":
            seq_cycles = c
        elif strategy == "fgmt":
            threads.append(t)
            cycles.append(c)

if not threads:
    raise SystemExit("No FGMT data found in summary CSV.")

pairs = sorted(zip(threads, cycles), key=lambda p: p[0])
threads = [p[0] for p in pairs]
cycles = [p[1] for p in pairs]

plt.figure(figsize=(10, 6))
plt.plot(threads, cycles, marker="o", linewidth=2.0, label="FGMT")
if seq_cycles is not None:
    plt.plot(
        threads,
        [seq_cycles for _ in threads],
        linestyle="--",
        color="black",
        linewidth=1.3,
        label="Sequential baseline",
    )
plt.xticks(threads, [str(t) for t in threads])
plt.xlabel("Thread count")
plt.ylabel("Mean total cycles")
plt.title("Heavy FGMT cycle trend")
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
