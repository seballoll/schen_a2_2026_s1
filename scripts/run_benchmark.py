#!/usr/bin/env python3
import argparse
import csv
import math
import os
import re
import statistics
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Dict, List, Optional, Tuple

WALL_RE = re.compile(r"Total wall \(ms\):\s*([0-9eE+\-.]+)")
CYCLES_RE = re.compile(r"Total cycles:\s*([0-9]+)")


@dataclass
class Config:
    name: str
    strategy: str
    threads: int
    quantum: Optional[int] = None


@dataclass
class RunRow:
    config_name: str
    strategy: str
    threads: int
    quantum: int
    run_index: int
    wall_ms: float
    total_cycles: int


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Run repeated SPH benchmark experiments.")
    parser.add_argument("--runner", required=True, help="Path to lab_runner executable.")
    parser.add_argument("--runs", type=int, default=200, help="Runs per configuration.")
    parser.add_argument("--steps", type=int, default=20, help="Steps per run.")
    parser.add_argument("--particles", type=int, default=200, help="Particles per run.")
    parser.add_argument(
        "--thread-counts",
        default="2,4",
        help="Comma-separated thread counts for parallel configs.",
    )
    parser.add_argument("--fgmt-quantum", type=int, default=16, help="FGMT simulated quantum.")
    parser.add_argument(
        "--out-dir",
        default="benchmark_artifacts",
        help="Directory where CSV/statistics/plots are generated.",
    )
    return parser.parse_args()


def make_configs(thread_counts: List[int], fgmt_quantum: int) -> List[Config]:
    configs = [Config(name="sequential_t1", strategy="sequential", threads=1, quantum=None)]
    for t in thread_counts:
        configs.append(Config(name=f"chunked_t{t}", strategy="chunked", threads=t, quantum=None))
        configs.append(Config(name=f"fgmt_t{t}", strategy="fgmt", threads=t, quantum=fgmt_quantum))
    return configs


def run_single(runner: str, steps: int, particles: int, cfg: Config) -> Tuple[float, int]:
    cmd = [runner, str(steps), str(particles), str(cfg.threads), cfg.strategy]
    if cfg.strategy == "fgmt":
        cmd.append(str(cfg.quantum if cfg.quantum is not None else 16))

    proc = subprocess.run(cmd, capture_output=True, text=True, check=True)
    text = proc.stdout

    wall_match = WALL_RE.search(text)
    cycles_match = CYCLES_RE.search(text)
    if wall_match is None or cycles_match is None:
        raise RuntimeError("Could not parse benchmark output.\nOutput:\n" + text)

    return float(wall_match.group(1)), int(cycles_match.group(1))


def write_raw_csv(rows: List[RunRow], path: Path) -> None:
    with path.open("w", newline="", encoding="utf-8") as f:
        writer = csv.writer(f)
        writer.writerow(
            [
                "config_name",
                "strategy",
                "threads",
                "quantum",
                "run_index",
                "wall_ms",
                "total_cycles",
            ]
        )
        for row in rows:
            writer.writerow(
                [
                    row.config_name,
                    row.strategy,
                    row.threads,
                    row.quantum,
                    row.run_index,
                    f"{row.wall_ms:.8f}",
                    row.total_cycles,
                ]
            )


def mean_std_ci(values: List[float]) -> Tuple[float, float, float]:
    if not values:
        return 0.0, 0.0, 0.0
    if len(values) == 1:
        return values[0], 0.0, 0.0
    mu = statistics.mean(values)
    sd = statistics.stdev(values)
    ci95 = 1.96 * sd / math.sqrt(len(values))
    return mu, sd, ci95


def group_rows(rows: List[RunRow]) -> Dict[str, List[RunRow]]:
    grouped: Dict[str, List[RunRow]] = {}
    for row in rows:
        grouped.setdefault(row.config_name, []).append(row)
    return grouped


def build_paired_speedups(group: List[RunRow], baseline_by_run: Dict[int, float]) -> List[float]:
    speedups: List[float] = []
    for row in group:
        base = baseline_by_run.get(row.run_index)
        if base is None or row.wall_ms <= 0.0:
            continue
        speedups.append(base / row.wall_ms)
    return speedups


def write_summary_and_convergence(rows: List[RunRow], out_dir: Path) -> None:
    grouped = group_rows(rows)

    baseline_rows = grouped.get("sequential_t1")
    if not baseline_rows:
        raise RuntimeError("Missing sequential_t1 baseline rows.")

    baseline_wall = [r.wall_ms for r in baseline_rows]
    baseline_cycles = [float(r.total_cycles) for r in baseline_rows]
    baseline_by_run = {r.run_index: r.wall_ms for r in baseline_rows}
    baseline_wall_mean = statistics.mean(baseline_wall)
    baseline_cycles_mean = statistics.mean(baseline_cycles)

    summary_path = out_dir / "summary_stats.csv"
    conv_path = out_dir / "convergence_report.txt"

    with summary_path.open("w", newline="", encoding="utf-8") as f:
        writer = csv.writer(f)
        writer.writerow(
            [
                "config_name",
                "strategy",
                "threads",
                "quantum",
                "runs",
                "wall_mean_ms",
                "wall_std_ms",
                "wall_ci95_ms",
                "cycles_mean",
                "cycles_std",
                "cycles_ci95",
                "speedup_time_mean",
                "speedup_time_std",
                "speedup_time_ci95",
                "speedup_cycles_mean",
                "speedup_cycles_std",
                "speedup_cycles_ci95",
                "wall_cv_percent",
            ]
        )

        lines = []
        lines.append("Convergence analysis (rule of thumb):")
        lines.append("- If CI95/mean > 2%, consider increasing runs.")
        lines.append("")

        for cfg_name in sorted(grouped.keys()):
            group = grouped[cfg_name]
            wall = [r.wall_ms for r in group]
            cycles = [float(r.total_cycles) for r in group]

            wall_mean, wall_std, wall_ci = mean_std_ci(wall)
            cyc_mean, cyc_std, cyc_ci = mean_std_ci(cycles)

            speedup_time_samples = build_paired_speedups(group, baseline_by_run)
            if not speedup_time_samples:
                speedup_time_samples = [baseline_wall_mean / v for v in wall if v > 0.0]
            speedup_cycles_samples = [baseline_cycles_mean / v for v in cycles if v > 0.0]

            sp_t_mean, sp_t_std, sp_t_ci = mean_std_ci(speedup_time_samples)
            sp_c_mean, sp_c_std, sp_c_ci = mean_std_ci(speedup_cycles_samples)

            cv = (wall_std / wall_mean * 100.0) if wall_mean > 0.0 else 0.0

            sample = group[0]
            writer.writerow(
                [
                    cfg_name,
                    sample.strategy,
                    sample.threads,
                    sample.quantum,
                    len(group),
                    f"{wall_mean:.8f}",
                    f"{wall_std:.8f}",
                    f"{wall_ci:.8f}",
                    f"{cyc_mean:.8f}",
                    f"{cyc_std:.8f}",
                    f"{cyc_ci:.8f}",
                    f"{sp_t_mean:.8f}",
                    f"{sp_t_std:.8f}",
                    f"{sp_t_ci:.8f}",
                    f"{sp_c_mean:.8f}",
                    f"{sp_c_std:.8f}",
                    f"{sp_c_ci:.8f}",
                    f"{cv:.4f}",
                ]
            )

            ratio = (wall_ci / wall_mean * 100.0) if wall_mean > 0.0 else 0.0
            stable = "OK" if ratio <= 2.0 else "MORE_RUNS_RECOMMENDED"
            lines.append(
                f"- {cfg_name}: mean={wall_mean:.5f} ms, std={wall_std:.5f}, "
                f"CI95={wall_ci:.5f} ({ratio:.2f}% of mean) -> {stable}"
            )

    conv_path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def generate_plots(rows: List[RunRow], out_dir: Path) -> None:
    try:
        import matplotlib.pyplot as plt
    except Exception as exc:  # pragma: no cover
        (out_dir / "plot_warning.txt").write_text(
            "Could not import matplotlib. Install it to generate plots.\n"
            f"Error: {exc}\n",
            encoding="utf-8",
        )
        return

    grouped = group_rows(rows)
    config_names = sorted(grouped.keys())

    wall_data = [[r.wall_ms for r in grouped[c]] for c in config_names]
    baseline_rows = grouped["sequential_t1"]
    baseline = statistics.mean([r.wall_ms for r in baseline_rows])
    baseline_by_run = {r.run_index: r.wall_ms for r in baseline_rows}
    speedup_data = []
    for c in config_names:
        samples = build_paired_speedups(grouped[c], baseline_by_run)
        if not samples:
            samples = [baseline / r.wall_ms for r in grouped[c] if r.wall_ms > 0.0]
        speedup_data.append(samples)

    plt.figure(figsize=(12, 6))
    plt.boxplot(wall_data, tick_labels=config_names, showmeans=True)
    plt.xticks(rotation=25, ha="right")
    plt.ylabel("Wall time (ms)")
    plt.title("Wall-time distribution by configuration")
    plt.tight_layout()
    plt.savefig(out_dir / "wall_time_boxplot.png", dpi=140)
    plt.close()

    plt.figure(figsize=(12, 6))
    plt.boxplot(speedup_data, tick_labels=config_names, showmeans=True)
    plt.xticks(rotation=25, ha="right")
    plt.ylabel("Speedup vs paired sequential run")
    plt.title("Speedup distribution by configuration (paired by run index)")
    plt.tight_layout()
    plt.savefig(out_dir / "speedup_boxplot.png", dpi=140)
    plt.close()

    warning_path = out_dir / "plot_warning.txt"
    if warning_path.exists():
        warning_path.unlink()

    for cfg in config_names:
        vals = [r.wall_ms for r in grouped[cfg]]
        plt.figure(figsize=(7, 4))
        plt.hist(vals, bins=20, edgecolor="black")
        plt.xlabel("Wall time (ms)")
        plt.ylabel("Frequency")
        plt.title(f"Histogram: {cfg}")
        plt.tight_layout()
        plt.savefig(out_dir / f"hist_{cfg}.png", dpi=140)
        plt.close()


def main() -> int:
    args = parse_args()

    runner_path = args.runner.replace("\\ ", " ")
    args.runner = runner_path

    out_dir = Path(args.out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)

    thread_counts = [int(v.strip()) for v in args.thread_counts.split(",") if v.strip()]
    configs = make_configs(thread_counts, args.fgmt_quantum)

    rows: List[RunRow] = []

    print(f"Running benchmark with {args.runs} repetitions per configuration...")
    for cfg in configs:
        print(f"  -> {cfg.name} ({cfg.strategy}, threads={cfg.threads}, quantum={cfg.quantum})")
        for run_idx in range(args.runs):
            wall_ms, total_cycles = run_single(args.runner, args.steps, args.particles, cfg)
            rows.append(
                RunRow(
                    config_name=cfg.name,
                    strategy=cfg.strategy,
                    threads=cfg.threads,
                    quantum=(cfg.quantum if cfg.quantum is not None else 0),
                    run_index=run_idx,
                    wall_ms=wall_ms,
                    total_cycles=total_cycles,
                )
            )

    raw_path = out_dir / "raw_runs.csv"
    write_raw_csv(rows, raw_path)

    write_summary_and_convergence(rows, out_dir)
    generate_plots(rows, out_dir)

    print("\nBenchmark completed.")
    print(f"- Raw runs: {raw_path}")
    print(f"- Summary: {out_dir / 'summary_stats.csv'}")
    print(f"- Convergence: {out_dir / 'convergence_report.txt'}")
    print(f"- Plots: {out_dir}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
