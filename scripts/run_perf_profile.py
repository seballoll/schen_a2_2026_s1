#!/usr/bin/env python3
import argparse
import csv
import glob
import math
import re
import statistics
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Dict, List, Optional, Tuple

WALL_RE = re.compile(r"Total wall \(ms\):\s*([0-9eE+\-.]+)")


@dataclass
class Config:
    name: str
    strategy: str
    threads: int


@dataclass
class PerfRow:
    config_name: str
    strategy: str
    threads: int
    run_index: int
    event: str
    value: float
    unit: str
    wall_ms: float


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Run direct machine profiling using perf stat for SMT/CMP and baseline."
    )
    parser.add_argument("--runner", required=True, help="Path to lab_runner executable.")
    parser.add_argument("--runs", type=int, default=10, help="Runs per configuration.")
    parser.add_argument("--steps", type=int, default=50, help="Steps per run.")
    parser.add_argument("--particles", type=int, default=1200, help="Particles per run.")
    parser.add_argument(
        "--thread-counts",
        default="2,4",
        help="Comma-separated thread counts for SMT/CMP configs.",
    )
    parser.add_argument(
        "--events",
        default=(
            "task-clock,cycles,instructions,branches,branch-misses,"
            "cache-references,cache-misses,context-switches,cpu-migrations,page-faults"
        ),
        help="Comma-separated perf events.",
    )
    parser.add_argument(
        "--perf-cmd",
        default="perf",
        help="Perf command path (e.g., perf).",
    )
    parser.add_argument(
        "--strict",
        action="store_true",
        help="Exit with non-zero status if perf is unavailable.",
    )
    parser.add_argument(
        "--out-dir",
        default="benchmark_artifacts/perf",
        help="Directory for profiling artifacts.",
    )
    return parser.parse_args()


def make_configs(thread_counts: List[int]) -> List[Config]:
    configs = [Config(name="sequential_t1", strategy="sequential", threads=1)]
    for t in thread_counts:
        configs.append(Config(name=f"smt_t{t}", strategy="smt", threads=t))
        configs.append(Config(name=f"cmp_t{t}", strategy="cmp", threads=t))
    return configs


def check_perf_available(perf_cmd: str) -> Tuple[bool, str]:
    probe = [perf_cmd, "stat", "-x,", "-e", "task-clock", "--", "true"]
    proc = subprocess.run(probe, capture_output=True, text=True)
    if proc.returncode == 0:
        return True, ""

    detail = (proc.stderr or proc.stdout or "unknown perf error").strip()
    return False, detail


def resolve_perf_command(preferred: str) -> Tuple[Optional[str], str]:
    candidates = [preferred]
    candidates.extend(sorted(glob.glob("/usr/lib/linux-tools-*/perf"), reverse=True))
    candidates.extend(sorted(glob.glob("/usr/lib/linux-hwe-*-tools-*/perf"), reverse=True))

    tried_details: List[str] = []
    seen = set()

    for candidate in candidates:
        if candidate in seen:
            continue
        seen.add(candidate)

        ok, detail = check_perf_available(candidate)
        if ok:
            return candidate, ""

        short_detail = detail.splitlines()[0] if detail else "unknown error"
        tried_details.append(f"- {candidate}: {short_detail}")

    return None, "\n".join(tried_details)


def parse_perf_csv(stderr_text: str) -> Dict[str, Tuple[float, str]]:
    metrics: Dict[str, Tuple[float, str]] = {}

    for raw in stderr_text.splitlines():
        line = raw.strip()
        if not line or line.startswith("#"):
            continue

        parts = [p.strip() for p in line.split(",")]
        if len(parts) < 3:
            continue

        value_str = parts[0]
        unit = parts[1]
        event = parts[2]

        if (
            not event
            or value_str in {"<not counted>", "not counted", "<not supported>", "not supported"}
            or "not supported" in value_str
        ):
            continue

        value_clean = value_str.replace(" ", "").replace(",", "")
        try:
            value = float(value_clean)
        except ValueError:
            continue

        metrics[event] = (value, unit)

    return metrics


def run_single(
    perf_cmd: str,
    events: str,
    runner: str,
    steps: int,
    particles: int,
    cfg: Config,
) -> Tuple[float, Dict[str, Tuple[float, str]]]:
    runner_cmd = [runner, str(steps), str(particles), str(cfg.threads), cfg.strategy]

    cmd = [
        perf_cmd,
        "stat",
        "-x,",
        "--no-big-num",
        "-e",
        events,
        "--",
        *runner_cmd,
    ]

    proc = subprocess.run(cmd, capture_output=True, text=True, check=True)

    wall_match = WALL_RE.search(proc.stdout)
    wall_ms = float(wall_match.group(1)) if wall_match else 0.0

    perf_metrics = parse_perf_csv(proc.stderr)
    return wall_ms, perf_metrics


def mean_std_ci(values: List[float]) -> Tuple[float, float, float]:
    if not values:
        return 0.0, 0.0, 0.0
    if len(values) == 1:
        return values[0], 0.0, 0.0
    mu = statistics.mean(values)
    sd = statistics.stdev(values)
    ci95 = 1.96 * sd / math.sqrt(len(values))
    return mu, sd, ci95


def write_raw_csv(rows: List[PerfRow], path: Path) -> None:
    with path.open("w", newline="", encoding="utf-8") as f:
        writer = csv.writer(f)
        writer.writerow([
            "config_name",
            "strategy",
            "threads",
            "run_index",
            "event",
            "value",
            "unit",
            "wall_ms",
        ])
        for row in rows:
            writer.writerow([
                row.config_name,
                row.strategy,
                row.threads,
                row.run_index,
                row.event,
                f"{row.value:.8f}",
                row.unit,
                f"{row.wall_ms:.8f}",
            ])


def write_summary_csv(rows: List[PerfRow], out_path: Path) -> None:
    grouped: Dict[Tuple[str, str], List[PerfRow]] = {}
    for row in rows:
        grouped.setdefault((row.config_name, row.event), []).append(row)

    with out_path.open("w", newline="", encoding="utf-8") as f:
        writer = csv.writer(f)
        writer.writerow([
            "config_name",
            "strategy",
            "threads",
            "event",
            "unit",
            "runs",
            "mean",
            "std",
            "ci95",
        ])

        for (config_name, event) in sorted(grouped.keys()):
            entries = grouped[(config_name, event)]
            values = [r.value for r in entries]
            mu, sd, ci = mean_std_ci(values)
            sample = entries[0]
            writer.writerow([
                config_name,
                sample.strategy,
                sample.threads,
                event,
                sample.unit,
                len(entries),
                f"{mu:.8f}",
                f"{sd:.8f}",
                f"{ci:.8f}",
            ])


def main() -> int:
    args = parse_args()

    out_dir = Path(args.out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)

    perf_cmd, resolution_detail = resolve_perf_command(args.perf_cmd)
    if perf_cmd is None:
        warning = (
            "perf is not available or not usable on this machine.\n"
            f"Requested command: {args.perf_cmd}\n"
            f"Tried:\n{resolution_detail}\n"
            "Tip: install linux-tools matching the running kernel.\n"
        )
        (out_dir / "perf_warning.txt").write_text(warning, encoding="utf-8")
        print(warning)
        return 2 if args.strict else 0

    args.perf_cmd = perf_cmd
    print(f"Using perf command: {args.perf_cmd}")

    thread_counts = [int(v.strip()) for v in args.thread_counts.split(",") if v.strip()]
    configs = make_configs(thread_counts)

    rows: List[PerfRow] = []
    print(f"Running perf profiling with {args.runs} runs per configuration...")

    for cfg in configs:
        print(f"  -> {cfg.name} ({cfg.strategy}, threads={cfg.threads})")
        for run_idx in range(args.runs):
            wall_ms, perf_metrics = run_single(
                perf_cmd=args.perf_cmd,
                events=args.events,
                runner=args.runner,
                steps=args.steps,
                particles=args.particles,
                cfg=cfg,
            )

            for event, (value, unit) in perf_metrics.items():
                rows.append(
                    PerfRow(
                        config_name=cfg.name,
                        strategy=cfg.strategy,
                        threads=cfg.threads,
                        run_index=run_idx,
                        event=event,
                        value=value,
                        unit=unit,
                        wall_ms=wall_ms,
                    )
                )

    raw_path = out_dir / "perf_raw.csv"
    summary_path = out_dir / "perf_summary.csv"
    write_raw_csv(rows, raw_path)
    write_summary_csv(rows, summary_path)

    print("\nPerf profiling completed.")
    print(f"- Raw: {raw_path}")
    print(f"- Summary: {summary_path}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
