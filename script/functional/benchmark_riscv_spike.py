#!/usr/bin/env python3
"""Compile SysY cases for RV64 and count committed user instructions with Spike."""

import argparse
import csv
import re
import subprocess
import time
from pathlib import Path


HISTOGRAM_RE = re.compile(r"^([0-9a-fA-F]+) ([0-9]+)$")
GLOBAL_RE = re.compile(r"^\s*\.globl\s+(\S+)\s*$")
INST_RE = re.compile(r"^\s+([a-z][a-z0-9_.]*)\b")


def run(command, *, timeout, stdin=None):
    return subprocess.run(
        command,
        input=stdin,
        capture_output=True,
        text=True,
        timeout=timeout,
        check=False,
    )


def source_functions(assembly_path):
    functions = []
    in_text = False
    for line in assembly_path.read_text(encoding="utf-8").splitlines():
        if line.strip() == ".text":
            in_text = True
            continue
        if not in_text:
            continue
        match = GLOBAL_RE.match(line)
        if match:
            functions.append(match.group(1))
    return functions


def text_symbols(nm, elf_path, timeout):
    result = run([nm, "-n", str(elf_path)], timeout=timeout)
    if result.returncode != 0:
        raise RuntimeError(result.stderr.strip())
    symbols = []
    for line in result.stdout.splitlines():
        fields = line.split()
        if len(fields) < 3 or fields[1] not in {"T", "t", "W", "w"}:
            continue
        try:
            address = int(fields[0], 16)
        except ValueError:
            continue
        symbols.append((address, fields[2]))
    return symbols


def function_ranges(functions, symbols):
    by_name = {name: address for address, name in symbols}
    ranges = []
    for name in functions:
        start = by_name.get(name)
        if start is None:
            continue
        end = next((address for address, _ in symbols if address > start), start + 1)
        ranges.append((start, end))
    return ranges


def instruction_count(stderr, ranges):
    total = 0
    for line in stderr.splitlines():
        match = HISTOGRAM_RE.match(line.strip())
        if not match:
            continue
        pc = int(match.group(1), 16)
        if any(start <= pc < end for start, end in ranges):
            total += int(match.group(2))
    return total


def static_instruction_count(assembly_path):
    total = 0
    for line in assembly_path.read_text(encoding="utf-8").splitlines():
        if INST_RE.match(line) and not line.lstrip().startswith("."):
            total += 1
    return total


def normalized_output(stdout, returncode):
    lines = [
        line.rstrip()
        for line in stdout.splitlines()
        if not line.startswith("Timer@") and not line.startswith("TOTAL:")
    ]
    lines.append(str(returncode))
    return "\n".join(lines).strip()


def expected_output(case_path):
    path = case_path.with_suffix(".out")
    if not path.exists():
        return None
    return "\n".join(line.rstrip() for line in path.read_text(encoding="utf-8").splitlines()).strip()


def collect_cases(inputs):
    cases = []
    for item in inputs:
        path = Path(item)
        if path.is_dir():
            cases.extend(sorted(path.glob("*.sy")))
        elif path.suffix == ".sy":
            cases.append(path)
    return cases


def benchmark_case(args, case_path, work_dir):
    assembly_path = work_dir / f"{case_path.stem}.s"
    elf_path = work_dir / f"{case_path.stem}.elf"
    compile_result = run(
        [str(args.compiler), str(case_path), "-S", "-riscv", args.opt, "-o", str(assembly_path)],
        timeout=args.compile_timeout,
    )
    if compile_result.returncode != 0:
        raise RuntimeError(f"compile: {compile_result.stderr.strip()}")
    link_result = run(
        [args.gcc, str(assembly_path), str(args.runtime), "-mcmodel=medany", "-o", str(elf_path)],
        timeout=args.compile_timeout,
    )
    if link_result.returncode != 0:
        raise RuntimeError(f"link: {link_result.stderr.strip()}")

    functions = source_functions(assembly_path)
    ranges = function_ranges(functions, text_symbols(args.nm, elf_path, args.compile_timeout))
    stdin_path = case_path.with_suffix(".in")
    stdin = stdin_path.read_text(encoding="utf-8") if stdin_path.exists() else None
    started = time.perf_counter()
    spike_result = run([args.spike, "-g", str(args.pk), str(elf_path)], timeout=args.run_timeout, stdin=stdin)
    elapsed = time.perf_counter() - started
    expected = expected_output(case_path)
    actual = normalized_output(spike_result.stdout, spike_result.returncode)
    output_matches = expected is None or actual == expected
    if not output_matches and not args.allow_output_mismatch:
        raise RuntimeError(
            f"output mismatch: expected {expected[:160]!r}, got {actual[:160]!r}"
        )
    return (
        instruction_count(spike_result.stderr, ranges),
        static_instruction_count(assembly_path),
        elapsed,
        output_matches,
    )


def main():
    script_dir = Path(__file__).resolve().parent
    repo_root = script_dir.parent.parent
    parser = argparse.ArgumentParser()
    parser.add_argument("inputs", nargs="+")
    parser.add_argument("--compiler", type=Path, default=repo_root / "bin" / "compiler")
    parser.add_argument("--runtime", type=Path, default=repo_root / "libs" / "librv.a")
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--work-dir", type=Path, default=repo_root / "testoutputs" / "spike" / "tmp")
    parser.add_argument("--opt", default="-O1")
    parser.add_argument("--gcc", default="riscv64-unknown-elf-gcc")
    parser.add_argument("--nm", default="riscv64-unknown-elf-nm")
    parser.add_argument("--spike", default="spike")
    parser.add_argument(
        "--pk",
        type=Path,
        default=Path("/opt/homebrew/opt/riscv-pk/riscv64-unknown-elf/bin/pk"),
    )
    parser.add_argument("--compile-timeout", type=int, default=90)
    parser.add_argument("--run-timeout", type=int, default=120)
    parser.add_argument(
        "--allow-output-mismatch",
        action="store_true",
        help="record instruction counts but mark cases whose output differs from the .out file",
    )
    args = parser.parse_args()

    args.work_dir.mkdir(parents=True, exist_ok=True)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    rows = []
    for case in collect_cases(args.inputs):
        try:
            dynamic, static, seconds, output_matches = benchmark_case(args, case, args.work_dir)
            status = "OK" if output_matches else "MISMATCH"
            rows.append([str(case), status, str(dynamic), str(static), f"{seconds:.6f}"])
            print(f"[{status}] {case.name}: dynamic={dynamic}, static={static}, spike={seconds:.3f}s")
        except subprocess.TimeoutExpired:
            rows.append([str(case), "TIMEOUT", "", "", ""])
            print(f"[TIMEOUT] {case.name}")
        except RuntimeError as error:
            rows.append([str(case), "ERROR", "", "", ""])
            print(f"[ERROR] {case.name}: {error}")

    with args.output.open("w", newline="", encoding="utf-8") as output:
        writer = csv.writer(output)
        writer.writerow(["case", "status", "dynamic_instructions", "static_instructions", "spike_seconds"])
        writer.writerows(rows)

    completed = [int(row[2]) for row in rows if row[1] in {"OK", "MISMATCH"}]
    if completed:
        print(f"completed={len(completed)}/{len(rows)}, total_dynamic={sum(completed)}")


if __name__ == "__main__":
    main()
