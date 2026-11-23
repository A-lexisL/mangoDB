#!/usr/bin/env python3
"""Select cases, check correctness, and compare mangoDB performance."""

from __future__ import annotations

import argparse
from collections import Counter
from dataclasses import dataclass
from datetime import datetime
import difflib
import fnmatch
import json
import os
from pathlib import Path
import platform
import re
import shutil
import signal
import subprocess
import sys
import tempfile


# Selection and command-line interface


def parse_args():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("queries", nargs="+", type=Path, help="query files or directories")
    parser.add_argument("--exclude", action="append", default=[], help="exclude filename or glob; repeatable")
    parser.add_argument("--binary", required=True, type=Path, help="candidate executable")
    parser.add_argument("--baseline", type=Path, help="baseline executable for a benchmark")
    parser.add_argument("--threads", type=int, default=0, help="candidate thread count (default: 0)")
    parser.add_argument("--baseline-threads", type=int, default=1, help="baseline thread count (default: 1)")
    parser.add_argument("--mode", choices=("correctness", "benchmark", "both"), default="both")
    parser.add_argument("--tool", choices=("time", "perf"), action="append", default=[], help="measurement tool; repeatable")
    parser.add_argument("--output-dir", required=True, type=Path, help="parent directory for a new report directory")
    parser.add_argument("--timeout", type=float, default=300, help="per-process timeout in seconds")
    return parser.parse_args()


def select_cases(paths: list[Path], exclusions: list[str]) -> list[Case]:
    """Resolve files and directories to distinct query cases."""
    selected = set()
    for path in paths:
        path = path.resolve()
        if path.is_dir():
            directory = path / "queries" if (path / "queries").is_dir() else path
            selected.update(directory.glob("*.query"))
        elif path.is_file() and path.suffix == ".query":
            selected.add(path)
        else:
            raise ValueError(f"Invalid query file or directory: {path}")
    cases = []
    for query in sorted(selected):
        if any(fnmatch.fnmatch(query.name, pattern) or fnmatch.fnmatch(query.stem, pattern)
               for pattern in exclusions):
            continue
        if query.parent.name != "queries":
            raise ValueError(f"Query must be inside a fixture's queries/ directory: {query}")
        cases.append(Case(query=query, fixture=query.parent.parent))
    if not cases:
        raise ValueError("No query files selected")
    return cases



# Case execution


@dataclass(frozen=True)
class Case:
    """A query and the fixture directory containing db, stdout, and dump."""

    query: Path
    fixture: Path

    @property
    def name(self) -> str:
        return self.query.stem


def run(case: Case, binary: Path, threads: int, run_dir: Path,
        tools: set[str], correctness: bool, timeout: float) -> dict:
    """Execute one program and collect its status, optional metrics, and checks."""
    run_dir.mkdir(parents=True)
    (run_dir / "db").symlink_to(case.fixture / "db", target_is_directory=True)
    for path in dumps(case.query):
        (run_dir / path).parent.mkdir(parents=True, exist_ok=True)
    command = [str(binary), f"--threads={threads}", "--listen", str(case.query)]
    if "time" in tools:
        command = ["/usr/bin/time", "-v", "-o", str(run_dir / "time.txt"), "--"] + command
    if "perf" in tools:
        command = ["perf", "stat", "-x,", "-e", "duration_time,user_time,system_time",
                   "-o", str(run_dir / "perf.csv"), "--"] + command
    result = {"binary": str(binary), "threads": threads, "command": command}
    with (run_dir / "stdout.txt").open("wb") as stdout, (run_dir / "stderr.txt").open("wb") as stderr:
        try:
            process = subprocess.Popen(command, cwd=run_dir, stdout=stdout, stderr=stderr,
                                       start_new_session=True)
            try:
                result["exit_code"] = process.wait(timeout=timeout)
            except subprocess.TimeoutExpired:
                os.killpg(process.pid, signal.SIGKILL)
                process.wait()
                result["exit_code"] = None
                result["error"] = f"Timed out after {timeout} seconds"
        except OSError as exc:
            result["exit_code"] = None
            result["error"] = str(exc)
    result["comparison_errors"] = check(case, run_dir) if correctness else []
    result["measurements"] = measurements(run_dir)
    return result


def profile(case: Case, binary: Path, threads: int, profile_dir: Path, timeout: float) -> dict:
    """Sample user-space CPU stacks in a separate run and summarize hotspots."""
    profile_dir.mkdir()
    (profile_dir / "db").symlink_to(case.fixture / "db", target_is_directory=True)
    for path in dumps(case.query):
        (profile_dir / path).parent.mkdir(parents=True, exist_ok=True)
    data = profile_dir / "perf.data"
    command = ["perf", "record", "-e", "cpu-clock:u", "-F", "99", "--call-graph", "dwarf",
               "-o", str(data), "--", str(binary), f"--threads={threads}", "--listen", str(case.query)]
    result = {"command": command, "hotspots": []}
    with (profile_dir / "stdout.txt").open("wb") as stdout, (profile_dir / "stderr.txt").open("wb") as stderr:
        try:
            process = subprocess.Popen(command, cwd=profile_dir, stdout=stdout, stderr=stderr,
                                       start_new_session=True)
            try:
                result["exit_code"] = process.wait(timeout=timeout)
            except subprocess.TimeoutExpired:
                os.killpg(process.pid, signal.SIGKILL)
                process.wait()
                result["exit_code"] = None
                result["error"] = f"Timed out after {timeout} seconds"
        except OSError as exc:
            result["exit_code"] = None
            result["error"] = str(exc)
    if result["exit_code"] == 0:
        analysis = subprocess.run(["perf", "report", "--stdio", "--no-children", "-g", "none",
                                   "--sort", "symbol,dso", "-i", str(data)],
                                  capture_output=True, text=True, check=False)
        (profile_dir / "hotspots.txt").write_text(analysis.stdout + analysis.stderr)
        result["report_exit_code"] = analysis.returncode
        result["hotspots"] = [re.sub(r"\s{2,}", "  ", line.strip()) for line in analysis.stdout.splitlines()
                              if re.match(r"\s*\d+(?:\.\d+)?%", line)][:5]
        sample_count = re.search(r"^# Samples:\s*(\S+)", analysis.stdout, re.M)
        if sample_count:
            result["sample_count"] = sample_count.group(1)
        if analysis.returncode:
            result["report_error"] = analysis.stderr.strip()
    return result



# Comparison and measurement utilities


def dumps(query: Path) -> list[Path]:
    """Return the distinct relative DUMP destinations used by a query."""
    pattern = r"^\s*DUMP\s+\S+\s+(\S+?)\s*;"
    paths = {Path(value) for value in re.findall(pattern, query.read_text(), re.I | re.M)}
    for path in paths:
        if path.is_absolute() or ".." in path.parts or not path.parts or path.parts[0] != "tmp":
            raise ValueError(f"DUMP destination must be under tmp/: {path}")
    return sorted(paths)


def table_rows(path: Path) -> tuple:
    """Read a table while ignoring row order and whitespace."""
    lines = [line.split() for line in path.read_text().splitlines()]
    if len(lines) < 2 or len(lines[0]) != 2 or not lines[0][1].isdigit():
        raise ValueError(f"Invalid table header: {path}")
    width = int(lines[0][1])
    if any(len(row) != width for row in lines[1:]):
        raise ValueError(f"Invalid table row width: {path}")
    return tuple(lines[0]), tuple(lines[1]), Counter(tuple(row) for row in lines[2:])


def check(case, run_dir: Path) -> list[str]:
    """Compare stdout and each expected dump; return descriptions of failures."""
    errors = []
    actual_stdout = run_dir / "stdout.txt"
    expected_stdout = case.fixture / "stdout" / f"{case.name}.out"
    if not expected_stdout.is_file():
        errors.append(f"Missing expected stdout: {expected_stdout}")
    elif actual_stdout.read_bytes() != expected_stdout.read_bytes():
        errors.append("Stdout differs from expected output")
        diff = difflib.unified_diff(expected_stdout.read_text(errors="replace").splitlines(True),
                                    actual_stdout.read_text(errors="replace").splitlines(True),
                                    fromfile=str(expected_stdout), tofile=str(actual_stdout))
        (run_dir / "stdout.diff").write_text("".join(diff))
    paths = dumps(case.query)
    official_dir = case.fixture / "dump" / case.name
    expected_files = (set(official_dir.glob("*.tbl")) if official_dir.is_dir()
                      else set((case.fixture / "dump").glob(f"{case.name}_*.tbl")))
    mapped = set()
    for path in paths:
        expected = (official_dir / path.name if official_dir.is_dir()
                    else case.fixture / "dump" / f"{case.name}_{path.name}")
        actual = run_dir / path
        mapped.add(expected)
        if not expected.is_file() or not actual.is_file():
            errors.append(f"Missing dump: expected={expected} actual={actual}")
            continue
        try:
            if table_rows(expected) != table_rows(actual):
                errors.append(f"Dump differs: {path}")
        except ValueError as exc:
            errors.append(str(exc))
    if expected_files != mapped:
        errors.append("DUMP paths do not match the expected table files")
    return errors


def measurements(run_dir: Path) -> dict:
    """Read the time and perf measurements produced for a run."""
    metrics = {}
    time_file = run_dir / "time.txt"
    if time_file.exists():
        fields = {"User time (seconds)": "user_s", "System time (seconds)": "system_s",
                  "Percent of CPU this job got": "cpu_percent",
                  "Maximum resident set size (kbytes)": "max_rss_kib",
                  "Voluntary context switches": "voluntary_context_switches",
                  "Involuntary context switches": "involuntary_context_switches"}
        for line in time_file.read_text().splitlines():
            line = line.strip()
            if line.startswith("Elapsed (wall clock) time"):
                clock = line.rsplit(": ", 1)[-1].split(":")
                metrics["wall_s"] = sum(float(value) * 60 ** index
                                        for index, value in enumerate(reversed(clock)))
            else:
                key, separator, value = line.partition(": ")
                if separator and key in fields:
                    metrics[fields[key]] = value.strip()
    perf_file = run_dir / "perf.csv"
    if perf_file.exists():
        for line in perf_file.read_text().splitlines():
            fields = line.split(",")
            event_names = {"duration_time": "perf_wall_s", "user_time": "perf_user_s",
                           "system_time": "perf_system_s"}
            if len(fields) > 2 and fields[2] in event_names and fields[0].isdigit():
                metrics[event_names[fields[2]]] = int(fields[0]) / 1_000_000_000
    return metrics



# Report output


def write_case(case_dir: Path, case: str, results: dict) -> None:
    """Write one structured report for a case and its program runs."""
    report = {"case": case, "runs": results}
    if "baseline" in results and "candidate" in results:
        baseline = results["baseline"]["measurements"]
        candidate = results["candidate"]["measurements"]
        key = "perf_wall_s" if "perf_wall_s" in baseline and "perf_wall_s" in candidate else "wall_s"
        if (results["baseline"]["exit_code"] == 0 and results["candidate"]["exit_code"] == 0
                and baseline.get(key) is not None and candidate.get(key)):
            report["speedup"] = baseline[key] / candidate[key]
    (case_dir / "report.json").write_text(json.dumps(report, indent=2) + "\n")


def write_summary(output, cases: list[dict]) -> None:
    groups = {"Faster": [], "Slower": [], "Equal": [], "Unavailable": []}
    baseline_total = candidate_total = 0.0
    timed_cases = 0
    for case in cases:
        results = case["results"]
        if "baseline" not in results:
            continue
        baseline = results["baseline"]["measurements"]
        candidate = results["candidate"]["measurements"]
        key = "perf_wall_s" if "perf_wall_s" in baseline and "perf_wall_s" in candidate else "wall_s"
        before, after = baseline.get(key), candidate.get(key)
        if before is None or not after:
            group = "Unavailable"
        else:
            baseline_total += before
            candidate_total += after
            timed_cases += 1
            group = "Faster" if after <= before else "Slower" if after > before else "Equal"
        query = Path(case["query"])
        groups[group].append(f"{query.parent.parent.name}/{query.stem}")
    if any(groups.values()):
        print("Performance summary (candidate vs baseline; wall time):", file=output)
        for group, names in groups.items():
            print(f"  {group} ({len(names)}): {', '.join(names) or 'none'}", file=output)
        if timed_cases:
            print(f"  Total wall time ({timed_cases} comparable cases): "
                  f"baseline={baseline_total:.6f}s, candidate={candidate_total:.6f}s", file=output)
            print(f"  Overall speedup: {baseline_total / candidate_total:.3f}x "
                  "(sum of case wall times)", file=output)
            difference = candidate_total - baseline_total
            direction = "slower" if difference > 0 else "faster" if difference < 0 else "equal"
            percent = (f" ({abs(difference) / baseline_total * 100:.2f}%)"
                       if baseline_total > 0 else "")
            print(f"  Total comparison: candidate {direction} by "
                  f"{abs(difference):.6f}s{percent}", file=output)
        else:
            print("  Total wall time: unavailable (no comparable cases)", file=output)
        print(file=output)


def write_report(root: Path, started: datetime, args, cases: list[dict]) -> None:
    """Write the run configuration and readable per-case results."""
    with (root / "report.txt").open("w") as output:
        def line(text=""):
            print(text, file=output)

        line("mangoDB test report")
        write_summary(output, cases)
        line(f"Started: {started.isoformat(timespec='seconds')}")
        line(f"Host: {platform.node()} | Kernel: {platform.release()}")
        line(f"Mode: {args.mode} | Tools: {', '.join(args.tool) or 'none'} | Timeout: {args.timeout}s")
        line(f"Candidate: {args.binary.resolve()} | Threads: {args.threads}")
        if args.baseline:
            line(f"Baseline: {args.baseline.resolve()} | Threads: {args.baseline_threads}")
        line(f"Inputs: {', '.join(str(path) for path in args.queries)}")
        line(f"Exclusions: {', '.join(args.exclude) or 'none'}")
        line(f"Cases: {len(cases)}")
        if "perf" in args.tool:
            line("Perf: time from perf stat; CPU hotspots sampled in separate executions (cpu-clock:u).")
        for case in cases:
            line()
            line(f"Case: {case['query']}")
            line(f"Artifacts: {case['directory']}")
            for label, result in case["results"].items():
                success = result["exit_code"] == 0 and not result["comparison_errors"]
                if "profile" in result:
                    success = success and result["profile"]["exit_code"] == 0
                status = "PASS" if success else "FAIL"
                check_status = ("not run" if args.mode == "benchmark" else
                                "pass" if not result["comparison_errors"] else "fail")
                line(f"  {label}: {status} | exit: {result['exit_code']} | correctness: {check_status}")
                if result.get("error"):
                    line(f"    Error: {result['error']}")
                for error in result["comparison_errors"]:
                    line(f"    Comparison: {error}")
                metrics = result["measurements"]
                if "time" in args.tool:
                    line("    time: wall={wall_s}s, user={user_s}s, system={system_s}s, CPU={cpu_percent}, "
                         "max RSS={max_rss_kib} KiB, context switches voluntary={voluntary_context_switches}, "
                         "involuntary={involuntary_context_switches}".format_map(
                             {key: metrics.get(key, 'N/A') for key in (
                                 'wall_s', 'user_s', 'system_s', 'cpu_percent', 'max_rss_kib',
                                 'voluntary_context_switches', 'involuntary_context_switches')}))
                if "perf" in args.tool:
                    values = [metrics.get(key) for key in ('perf_wall_s', 'perf_user_s', 'perf_system_s')]
                    values = [f"{value:.6f}" if value is not None else 'N/A' for value in values]
                    line(f"    perf stat: wall={values[0]}s, user={values[1]}s, system={values[2]}s")
                    sampling = result.get("profile", {})
                    line(f"    perf record: exit={sampling.get('exit_code', 'N/A')}, "
                         f"samples={sampling.get('sample_count', 'N/A')}")
                    if sampling.get("error"):
                        line(f"      Error: {sampling['error']}")
                    if sampling.get("report_error"):
                        line(f"      Report: {sampling['report_error']}")
                    if sampling.get("hotspots"):
                        line("      Top on-CPU samples (self overhead, symbol, object):")
                        for hotspot in sampling["hotspots"]:
                            line(f"      {hotspot}")
                    else:
                        line("      No hotspots available; see profile/hotspots.txt and stderr.txt.")
            if 'baseline' in case['results']:
                baseline, candidate = case['results']['baseline'], case['results']['candidate']
                b, c = baseline['measurements'], candidate['measurements']
                key = 'perf_wall_s' if 'perf_wall_s' in b and 'perf_wall_s' in c else 'wall_s'
                if baseline['exit_code'] == 0 and candidate['exit_code'] == 0 and b.get(key) is not None and c.get(key):
                    line(f"  Comparison: {b[key] / c[key]:.2f}x speedup; "
                         f"candidate {'faster' if c[key] < b[key] else 'slower' if c[key] > b[key] else 'equal'} "
                         f"by {abs(c[key] - b[key]):.3f}s ({'perf' if key == 'perf_wall_s' else 'time'} wall clock)")
                else:
                    line("  Comparison: unavailable")



# Run orchestration


def main() -> int:
    args = parse_args()
    try:
        cases = select_cases(args.queries, args.exclude)
        for binary in (args.binary, args.baseline):
            if binary is not None and not binary.resolve().is_file():
                raise ValueError(f"Executable not found: {binary}")
        if args.mode in ("benchmark", "both") and not args.baseline:
            raise ValueError("Benchmark mode requires --baseline")
        if args.threads < 0 or args.baseline_threads < 0 or args.timeout <= 0:
            raise ValueError("Thread counts must be nonnegative and timeout must be positive")
        tools = set(args.tool)
        if args.mode in ("benchmark", "both") and not tools:
            raise ValueError("Benchmark mode requires at least one --tool")
        for tool in tools:
            if shutil.which("/usr/bin/time" if tool == "time" else tool) is None:
                raise ValueError(f"Measurement tool not found: {tool}")
        if "perf" in tools:
            probe = subprocess.run(["perf", "stat", "-e", "duration_time,user_time,system_time", "--", "true"],
                                   capture_output=True, text=True, check=False)
            if probe.returncode:
                raise ValueError(f"Requested perf events are unavailable: {probe.stderr.strip()}")
        args.output_dir.mkdir(parents=True, exist_ok=True)
        started = datetime.now().astimezone()
        root = Path(tempfile.mkdtemp(prefix=started.strftime("%Y-%m-%d_%H-%M-%S-"),
                                     dir=args.output_dir.resolve()))
        reports = []
        failed = False
        for index, case in enumerate(cases, start=1):
            case_dir = root / f"{index:03d}-{case.fixture.name}-{case.name}"
            results = {}
            versions = [("candidate", args.binary, args.threads)]
            if args.mode in ("benchmark", "both"):
                versions.insert(0, ("baseline", args.baseline, args.baseline_threads))
            for label, binary, threads in versions:
                print(f"Running {case.query}: {label}", flush=True)
                result = run(case, binary.resolve(), threads, case_dir / label, tools,
                             args.mode in ("correctness", "both"), args.timeout)
                if "perf" in tools:
                    print(f"Profiling {case.query}: {label}", flush=True)
                    result["profile"] = profile(case, binary.resolve(), threads,
                                                case_dir / label / "profile", args.timeout)
                results[label] = result
                success = result["exit_code"] == 0 and not result["comparison_errors"]
                if "perf" in tools:
                    success = success and result["profile"]["exit_code"] == 0
                failed |= not success
            write_case(case_dir, str(case.query), results)
            reports.append({"query": str(case.query), "directory": str(case_dir), "results": results})
        write_report(root, started, args, reports)
        print(f"Report: {root / 'report.txt'}")
        return 1 if failed else 0
    except (ValueError, OSError) as exc:
        print(f"Error: {exc}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main())
