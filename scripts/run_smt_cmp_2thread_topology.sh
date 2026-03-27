#!/usr/bin/env bash
set -euo pipefail

# Usage:
#   ./scripts/run_smt_cmp_2thread_topology.sh [runner] [out_dir] [runs] [steps] [particles] [smt_cpus] [cmp_cpus] [toggle_smt]
#
# Example (auto-detect CPU sets):
#   ./scripts/run_smt_cmp_2thread_topology.sh \
#       "build_root_test/Proyecto Individual/lab_runner" \
#       benchmark_artifacts/smt_cmp_2thread_topology 20 50 1200
#
# Example (manual pinning):
#   ./scripts/run_smt_cmp_2thread_topology.sh \
#       "build_root_test/Proyecto Individual/lab_runner" \
#       benchmark_artifacts/smt_cmp_2thread_topology 20 50 1200 "0,1" "0,2"
#
# Example (explicit SMT toggle policy):
#   ./scripts/run_smt_cmp_2thread_topology.sh \
#       "build_root_test/Proyecto Individual/lab_runner" \
#       benchmark_artifacts/smt_cmp_2thread_topology 20 50 1200 "" "" yes

RUNNER="${1:-build_root_test/Proyecto Individual/lab_runner}"
OUT_DIR="${2:-benchmark_artifacts/smt_cmp_2thread_topology}"
RUNS="${3:-20}"
STEPS="${4:-50}"
PARTICLES="${5:-1200}"
SMT_CPUS="${6:-}"
CMP_CPUS="${7:-}"
TOGGLE_SMT="${8:-yes}"
SMT_CONTROL_PATH="/sys/devices/system/cpu/smt/control"
INITIAL_SMT_STATE=""

if [[ ! -x "$RUNNER" ]]; then
  echo "Runner not found or not executable: $RUNNER" >&2
  exit 1
fi

if ! command -v taskset >/dev/null 2>&1; then
  echo "taskset command not found. Install util-linux." >&2
  exit 1
fi

read_smt_state() {
    if [[ -r "$SMT_CONTROL_PATH" ]]; then
        cat "$SMT_CONTROL_PATH"
    else
        echo "unknown"
    fi
}

set_smt_state() {
    local state="$1"
    if [[ "$state" != "on" && "$state" != "off" ]]; then
        echo "Invalid SMT state: $state" >&2
        exit 1
    fi

    if [[ ! -e "$SMT_CONTROL_PATH" ]]; then
        echo "SMT control path not found: $SMT_CONTROL_PATH" >&2
        exit 1
    fi

    if [[ -w "$SMT_CONTROL_PATH" ]]; then
        printf "%s\n" "$state" > "$SMT_CONTROL_PATH"
    else
        printf "%s\n" "$state" | sudo -n tee "$SMT_CONTROL_PATH" >/dev/null
    fi
}

restore_initial_smt_state() {
    if [[ "$TOGGLE_SMT" == "yes" && "$INITIAL_SMT_STATE" != "" && "$INITIAL_SMT_STATE" != "unknown" ]]; then
        set_smt_state "$INITIAL_SMT_STATE" || true
    fi
}

trap restore_initial_smt_state EXIT

if [[ "$TOGGLE_SMT" == "yes" ]]; then
    INITIAL_SMT_STATE="$(read_smt_state)"
    # Ensure sibling threads are available for the SMT phase.
    if [[ "$INITIAL_SMT_STATE" != "on" ]]; then
        set_smt_state "on"
    fi
fi

if [[ -z "$SMT_CPUS" || -z "$CMP_CPUS" ]]; then
  mapfile -t AUTO_CPUSETS < <(python3 - <<'PY'
import subprocess
import sys
from collections import defaultdict

try:
    out = subprocess.check_output(["lscpu", "-p=CPU,CORE,SOCKET"], text=True)
except Exception as exc:
    print(f"ERROR: could not execute lscpu ({exc})", file=sys.stderr)
    sys.exit(2)

rows = []
for line in out.splitlines():
    if not line or line.startswith("#"):
        continue
    cpu_s, core_s, sock_s = [x.strip() for x in line.split(",")[:3]]
    rows.append((int(cpu_s), int(core_s), int(sock_s)))

if not rows:
    print("ERROR: empty CPU topology from lscpu", file=sys.stderr)
    sys.exit(2)

rows.sort(key=lambda t: t[0])
by_core = defaultdict(list)
for cpu, core, sock in rows:
    by_core[(sock, core)].append(cpu)

smt_core = None
for key in sorted(by_core.keys()):
    if len(by_core[key]) >= 2:
        smt_core = key
        break

if smt_core is None:
    print("ERROR: no core with at least two logical siblings (SMT appears disabled)", file=sys.stderr)
    sys.exit(2)

smt_cpu_a, smt_cpu_b = sorted(by_core[smt_core])[:2]
smt_set = f"{smt_cpu_a},{smt_cpu_b}"

cmp_core = None
for key in sorted(by_core.keys()):
    if key == smt_core:
        continue
    cmp_core = key
    break

if cmp_core is None:
    print("ERROR: no second physical core found for CMP comparison", file=sys.stderr)
    sys.exit(2)

cmp_cpu_a = smt_cpu_a
cmp_cpu_b = sorted(by_core[cmp_core])[0]
cmp_set = f"{cmp_cpu_a},{cmp_cpu_b}"

print(smt_set)
print(cmp_set)
PY
)

  if [[ ${#AUTO_CPUSETS[@]} -lt 2 ]]; then
    echo "Could not auto-detect CPU sets for SMT/CMP comparison." >&2
    exit 1
  fi

  [[ -z "$SMT_CPUS" ]] && SMT_CPUS="${AUTO_CPUSETS[0]}"
  [[ -z "$CMP_CPUS" ]] && CMP_CPUS="${AUTO_CPUSETS[1]}"
fi

mkdir -p "$OUT_DIR"
RAW_CSV="$OUT_DIR/raw_smt_cmp_2thread.csv"
SUMMARY_CSV="$OUT_DIR/summary_smt_cmp_2thread.csv"
PLOT_WALL="$OUT_DIR/smt_cmp_2thread_wall.png"
PLOT_CYCLES="$OUT_DIR/smt_cmp_2thread_cycles.png"

printf "model,placement,cpus,run_index,wall_ms,total_cycles\n" > "$RAW_CSV"

run_and_capture() {
  local model="$1"
  local placement="$2"
  local cpus="$3"
  local run_idx="$4"
  local output
  local wall
  local cycles

  output="$(taskset -c "$cpus" "$RUNNER" "$STEPS" "$PARTICLES" 2 "$model")"

  wall="$(printf "%s\n" "$output" | sed -n 's/^Total wall (ms):[[:space:]]*//p' | tail -1)"
  cycles="$(printf "%s\n" "$output" | sed -n 's/^Total cycles:[[:space:]]*//p' | tail -1)"

  if [[ -z "$wall" || -z "$cycles" ]]; then
    echo "Could not parse runner output for $model placement=$placement run=$run_idx" >&2
    echo "$output" >&2
    exit 1
  fi

    # Quote CPU set because it contains commas (e.g., "0,6") and must remain a single CSV field.
    printf "%s,%s,\"%s\",%s,%s,%s\n" "$model" "$placement" "$cpus" "$run_idx" "$wall" "$cycles" >> "$RAW_CSV"
}

echo "Running 2-thread topology comparison..."
echo "- runner: $RUNNER"
echo "- runs: $RUNS"
echo "- steps: $STEPS"
echo "- particles: $PARTICLES"
echo "- SMT 2-thread cpuset (same physical core): $SMT_CPUS"
echo "- CMP 2-thread cpuset (different physical cores): $CMP_CPUS"
echo "- toggle SMT between phases: $TOGGLE_SMT"

for ((i=0; i<RUNS; i++)); do
  run_and_capture "smt" "same_core" "$SMT_CPUS" "$i"
done

if [[ "$TOGGLE_SMT" == "yes" ]]; then
    set_smt_state "off"
fi

for ((i=0; i<RUNS; i++)); do
  run_and_capture "cmp" "different_cores" "$CMP_CPUS" "$i"
done

python3 - "$RAW_CSV" "$SUMMARY_CSV" "$PLOT_WALL" "$PLOT_CYCLES" <<'PY'
import csv
import math
import statistics
import sys
from pathlib import Path

raw_path = Path(sys.argv[1])
summary_path = Path(sys.argv[2])
plot_wall = Path(sys.argv[3])
plot_cycles = Path(sys.argv[4])

rows = []
with raw_path.open("r", encoding="utf-8") as f:
    reader = csv.DictReader(f)
    for r in reader:
        rows.append(
            {
                "model": r["model"],
                "placement": r["placement"],
                "cpus": r["cpus"],
                "run_index": int(r["run_index"]),
                "wall_ms": float(r["wall_ms"]),
                "total_cycles": float(r["total_cycles"]),
            }
        )

if not rows:
    raise SystemExit("No rows in raw CSV.")

groups = {}
for r in rows:
    key = (r["model"], r["placement"], r["cpus"])
    groups.setdefault(key, []).append(r)

def mean_std_ci(vals):
    if not vals:
        return 0.0, 0.0, 0.0
    if len(vals) == 1:
        return vals[0], 0.0, 0.0
    mu = statistics.mean(vals)
    sd = statistics.stdev(vals)
    ci95 = 1.96 * sd / math.sqrt(len(vals))
    return mu, sd, ci95

summary_rows = []
for key, g in sorted(groups.items(), key=lambda kv: (kv[0][0], kv[0][1], kv[0][2])):
    wall_vals = [x["wall_ms"] for x in g]
    cyc_vals = [x["total_cycles"] for x in g]
    wall_mu, wall_sd, wall_ci = mean_std_ci(wall_vals)
    cyc_mu, cyc_sd, cyc_ci = mean_std_ci(cyc_vals)
    summary_rows.append(
        {
            "model": key[0],
            "placement": key[1],
            "cpus": key[2],
            "runs": len(g),
            "wall_mean_ms": wall_mu,
            "wall_std_ms": wall_sd,
            "wall_ci95_ms": wall_ci,
            "cycles_mean": cyc_mu,
            "cycles_std": cyc_sd,
            "cycles_ci95": cyc_ci,
        }
    )

with summary_path.open("w", newline="", encoding="utf-8") as f:
    writer = csv.writer(f)
    writer.writerow(
        [
            "model",
            "placement",
            "cpus",
            "runs",
            "wall_mean_ms",
            "wall_std_ms",
            "wall_ci95_ms",
            "cycles_mean",
            "cycles_std",
            "cycles_ci95",
        ]
    )
    for r in summary_rows:
        writer.writerow(
            [
                r["model"],
                r["placement"],
                r["cpus"],
                r["runs"],
                f"{r['wall_mean_ms']:.8f}",
                f"{r['wall_std_ms']:.8f}",
                f"{r['wall_ci95_ms']:.8f}",
                f"{r['cycles_mean']:.8f}",
                f"{r['cycles_std']:.8f}",
                f"{r['cycles_ci95']:.8f}",
            ]
        )

try:
    import matplotlib.pyplot as plt
except Exception as exc:
    print(f"Plots skipped: matplotlib unavailable ({exc})")
    raise SystemExit(0)

labels = []
wall_means = []
wall_err = []
cyc_means = []
cyc_err = []

for r in summary_rows:
    label = f"{r['model'].upper()}\n{r['placement']}\nCPU {r['cpus']}"
    labels.append(label)
    wall_means.append(r["wall_mean_ms"])
    wall_err.append(r["wall_ci95_ms"])
    cyc_means.append(r["cycles_mean"])
    cyc_err.append(r["cycles_ci95"])

x = list(range(len(labels)))

plt.figure(figsize=(9, 6))
plt.bar(x, wall_means, yerr=wall_err, capsize=6)
plt.xticks(x, labels)
plt.ylabel("Mean wall time (ms)")
plt.title("2-thread topology comparison: SMT same-core vs CMP different-cores")
plt.grid(axis="y", alpha=0.3)
plt.tight_layout()
plt.savefig(plot_wall, dpi=160)
plt.close()

plt.figure(figsize=(9, 6))
plt.bar(x, cyc_means, yerr=cyc_err, capsize=6)
plt.xticks(x, labels)
plt.ylabel("Mean total cycles")
plt.title("2-thread topology comparison (cycles)")
plt.grid(axis="y", alpha=0.3)
plt.tight_layout()
plt.savefig(plot_cycles, dpi=160)
plt.close()

print(f"Plots generated: {plot_wall} | {plot_cycles}")
PY

echo "Done."
echo "- Raw CSV: $RAW_CSV"
echo "- Summary: $SUMMARY_CSV"
echo "- Plot wall: $PLOT_WALL"
echo "- Plot cycles: $PLOT_CYCLES"
