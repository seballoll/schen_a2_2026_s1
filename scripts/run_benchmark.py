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
        default="2,4,6,8,10,12,14,16,18",
        help="Comma-separated thread counts for parallel configs.",
    )
    parser.add_argument("--fgmt-quantum", type=int, default=16, help="FGMT simulated quantum.")
    parser.add_argument(
        "--out-dir",
        default="benchmark_artifacts",
        help="Directory where CSV/statistics/plots are generated.",
    )
    parser.add_argument(
        "--cmp-disable-smt",
        action="store_true",
        help="Disable SMT while running CMP configurations, then re-enable at the end.",
    )
    parser.add_argument(
        "--smt-control-path",
        default="/sys/devices/system/cpu/smt/control",
        help="Linux sysfs path to SMT control.",
    )
    parser.add_argument(
        "--smt-use-sudo",
        action="store_true",
        help="Use sudo -n to write SMT control (recommended for non-root runs).",
    )
    return parser.parse_args()


def make_configs(thread_counts: List[int], fgmt_quantum: int) -> List[Config]:
    configs = [Config(name="sequential_t1", strategy="sequential", threads=1, quantum=None)]
    for t in thread_counts:
        configs.append(Config(name=f"chunked_t{t}", strategy="chunked", threads=t, quantum=None))
        configs.append(Config(name=f"fgmt_t{t}", strategy="fgmt", threads=t, quantum=fgmt_quantum))
        configs.append(Config(name=f"smt_t{t}", strategy="smt", threads=t, quantum=None))
    for t in thread_counts:
        configs.append(Config(name=f"cmp_t{t}", strategy="cmp", threads=t, quantum=None))
    return configs


def set_smt_state(state: str, control_path: str, use_sudo: bool) -> None:
    if state not in {"on", "off"}:
        raise ValueError(f"Invalid SMT state: {state}")
    if not os.path.exists(control_path):
        raise RuntimeError(f"SMT control path not found: {control_path}")

    cmd: List[str] = []
    if use_sudo:
        cmd.extend(["sudo", "-n"])
    cmd.extend(["tee", control_path])

    proc = subprocess.run(
        cmd,
        input=state + "\n",
        text=True,
        capture_output=True,
        check=False,
    )
    if proc.returncode != 0:
        stderr = (proc.stderr or "").strip()
        stdout = (proc.stdout or "").strip()
        details = stderr if stderr else stdout
        raise RuntimeError(
            "Failed to set SMT state to "
            f"'{state}' using {' '.join(cmd)}. Details: {details}"
        )


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

    baseline_rows = grouped.get("sequential_t1", [])
    if not baseline_rows:
        (out_dir / "plot_warning.txt").write_text(
            "Missing sequential_t1 baseline rows. Focused plots were not generated.\n",
            encoding="utf-8",
        )
        return

    baseline_cycles_mean = statistics.mean([float(r.total_cycles) for r in baseline_rows])
    baseline_wall_mean = statistics.mean([r.wall_ms for r in baseline_rows])

    thread_counts = sorted({r.threads for r in rows if r.strategy != "sequential"})

    def mean_metric(strategy: str, threads: int, metric: str) -> Optional[float]:
        vals: List[float] = []
        for r in rows:
            if r.strategy == strategy and r.threads == threads:
                vals.append(float(r.total_cycles) if metric == "cycles" else r.wall_ms)
        if not vals:
            return None
        return statistics.mean(vals)

    fgmt_cycles: List[float] = []
    cgmt_cycles: List[float] = []
    smt_wall: List[float] = []
    cmp_wall: List[float] = []
    threads_for_cycle_plot: List[int] = []
    threads_for_wall_plot: List[int] = []

    for t in thread_counts:
        fg = mean_metric("fgmt", t, "cycles")
        cg = mean_metric("chunked", t, "cycles")
        if fg is not None and cg is not None:
            threads_for_cycle_plot.append(t)
            fgmt_cycles.append(fg)
            cgmt_cycles.append(cg)

        smt_v = mean_metric("smt", t, "wall")
        cmp_v = mean_metric("cmp", t, "wall")
        if smt_v is not None and cmp_v is not None:
            threads_for_wall_plot.append(t)
            smt_wall.append(smt_v)
            cmp_wall.append(cmp_v)

    if threads_for_cycle_plot:
        baseline_cycles = [baseline_cycles_mean for _ in threads_for_cycle_plot]

        plt.figure(figsize=(10, 6))
        plt.plot(threads_for_cycle_plot, fgmt_cycles, marker="o", linewidth=2.0, label="FGMT")
        plt.plot(threads_for_cycle_plot, cgmt_cycles, marker="s", linewidth=2.0, label="CGMT (chunked)")
        plt.plot(threads_for_cycle_plot, baseline_cycles, linestyle="--", linewidth=1.3, color="black", label="Sequential baseline")
        plt.xticks(threads_for_cycle_plot, [str(t) for t in threads_for_cycle_plot])
        plt.xlabel("Thread count")
        plt.ylabel("Mean total cycles")
        plt.title("Cycle trend vs thread count: FGMT and CGMT vs Sequential")
        plt.grid(axis="y", alpha=0.3)
        plt.legend()
        plt.tight_layout()
        plt.savefig(out_dir / "cycles_trend_fgmt_cgmt_vs_sequential.png", dpi=160)
        plt.close()

    if threads_for_wall_plot:
        baseline_wall = [baseline_wall_mean for _ in threads_for_wall_plot]

        plt.figure(figsize=(10, 6))
        plt.plot(threads_for_wall_plot, smt_wall, marker="o", linewidth=2.0, label="SMT")
        plt.plot(threads_for_wall_plot, cmp_wall, marker="s", linewidth=2.0, label="CMP")
        plt.plot(
            threads_for_wall_plot,
            baseline_wall,
            linestyle="--",
            linewidth=1.3,
            color="black",
            label="Sequential baseline",
        )
        plt.xticks(threads_for_wall_plot, [str(t) for t in threads_for_wall_plot])
        plt.xlabel("Thread count")
        plt.ylabel("Mean wall time (ms)")
        plt.title("Execution time trend vs thread count: SMT vs CMP")
        plt.grid(axis="y", alpha=0.3)
        plt.legend()
        plt.tight_layout()
        plt.savefig(out_dir / "wall_time_trend_smt_cmp.png", dpi=160)
        plt.close()

    warning_path = out_dir / "plot_warning.txt"
    if warning_path.exists():
        warning_path.unlink()

    # Focused figures only: cycle trend for FGMT/CGMT and wall-time trend for SMT/CMP.


def main() -> int:
    args = parse_args()

    runner_path = args.runner.replace("\\ ", " ")
    args.runner = runner_path

    out_dir = Path(args.out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)

    thread_counts = [int(v.strip()) for v in args.thread_counts.split(",") if v.strip()]
    configs = make_configs(thread_counts, args.fgmt_quantum)

    rows: List[RunRow] = []

    def run_config(cfg: Config) -> None:
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

    print(f"Running benchmark with {args.runs} repetitions per configuration...")

    non_cmp_configs = [cfg for cfg in configs if cfg.strategy != "cmp"]
    cmp_configs = [cfg for cfg in configs if cfg.strategy == "cmp"]

    for cfg in non_cmp_configs:
        run_config(cfg)

    if cmp_configs:
        if args.cmp_disable_smt:
            print("  -> Disabling SMT before CMP runs...")
            set_smt_state("off", args.smt_control_path, args.smt_use_sudo)
        try:
            for cfg in cmp_configs:
                run_config(cfg)
        finally:
            if args.cmp_disable_smt:
                print("  -> Re-enabling SMT after CMP runs...")
                set_smt_state("on", args.smt_control_path, args.smt_use_sudo)

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
