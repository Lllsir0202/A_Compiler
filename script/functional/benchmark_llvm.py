import argparse
import csv
import shutil
import subprocess
import time
from pathlib import Path

def run_command(command, timeout=None, stdin_path=None, stdout_path=None):
    stdin_file = None
    stdout_file = None
    try:
        if stdin_path is not None:
            stdin_file = open(stdin_path, "r", encoding="utf-8")
        if stdout_path is not None:
            stdout_file = open(stdout_path, "w", encoding="utf-8")
        start = time.perf_counter_ns()
        result = subprocess.run(
            command,
            stdin=stdin_file,
            stdout=stdout_file if stdout_file is not None else subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            timeout=timeout,
            check=False,
        )
        end = time.perf_counter_ns()
        return result, (end - start) / 1_000_000.0
    finally:
        if stdin_file is not None:
            stdin_file.close()
        if stdout_file is not None:
            stdout_file.close()

def append_return_code(output_path, return_code):
    with open(output_path, "a+", encoding="utf-8") as output_file:
        output_file.seek(0)
        content = output_file.read()
        if content and not content.endswith("\n"):
            output_file.write("\n")
        output_file.write(f"{return_code}\n")

def diff_output(expected_output, actual_output):
    result = subprocess.run(
        ["diff", "--strip-trailing-cr", str(expected_output), str(actual_output), "-b"],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        check=False,
    )
    return result.returncode == 0

def write_table(table_path, headers, rows):
    with open(table_path, "w", newline="", encoding="utf-8") as csv_file:
        writer = csv.writer(csv_file)
        writer.writerow(headers)
        writer.writerows(rows)

def write_markdown_table(table_path, headers, rows, title):
    with open(table_path, "w", encoding="utf-8") as markdown_file:
        markdown_file.write(f"# {title}\n\n")
        markdown_file.write("| " + " | ".join(headers) + " |\n")
        markdown_file.write("| " + " | ".join(["---"] * len(headers)) + " |\n")
        for row in rows:
            markdown_file.write("| " + " | ".join(row) + " |\n")

def read_existing_rows(table_path):
    if not table_path.exists():
        return None, []
    with open(table_path, "r", newline="", encoding="utf-8") as csv_file:
        reader = csv.reader(csv_file)
        rows = list(reader)
    if not rows:
        return None, []
    return rows[0], rows[1:]

def merge_rows(table_path, headers, new_rows):
    existing_headers, existing_rows = read_existing_rows(table_path)
    if existing_headers is not None and existing_headers != headers:
        raise RuntimeError(f"table header mismatch: {table_path}")
    return existing_rows + new_rows

def normalize_flag(flag):
    return flag.replace("-", "").replace(" ", "_")

def benchmark_case(compiler, case_path, stage_flag, opt_flag, arch, libs_dir, work_dir, timeout_compile, timeout_run):
    case_name = case_path.stem
    llvm_output = work_dir / f"{case_name}.ll"
    object_output = work_dir / f"{case_name}.o"
    binary_output = work_dir / f"{case_name}.out"
    program_output = work_dir / f"{case_name}.program.out"
    stdin_path = case_path.with_suffix(".in")
    expected_output = case_path.with_suffix(".out")

    compile_result, compile_ms = run_command(
        [str(compiler), str(case_path), stage_flag, "-o", str(llvm_output), opt_flag],
        timeout=timeout_compile,
    )
    if compile_result.returncode != 0:
        raise RuntimeError(f"compile failed: {compile_result.stderr.strip()}")

    clang_result, _ = run_command(
        ["clang", str(llvm_output), "-c", "-o", str(object_output), "-w"],
        timeout=timeout_compile,
    )
    if clang_result.returncode != 0:
        raise RuntimeError(f"clang failed: {clang_result.stderr.strip()}")

    lib_map = {"x86": "libx86.a", "arm": "libarm.a", "rv": "librv.a"}
    if arch == "x86":
        link_command = ["clang", "-static", str(object_output), "-L", str(libs_dir), f"-l:{lib_map[arch]}", "-o",
                        str(binary_output)]
    else:
        link_command = ["clang", str(object_output), str(libs_dir / lib_map[arch]), "-o", str(binary_output)]

    link_result, _ = run_command(link_command, timeout=timeout_compile)
    if link_result.returncode != 0:
        raise RuntimeError(f"link failed: {link_result.stderr.strip()}")

    run_result, run_ms = run_command(
        [str(binary_output)],
        timeout=timeout_run,
        stdin_path=stdin_path if stdin_path.exists() else None,
        stdout_path=program_output,
    )
    append_return_code(program_output, run_result.returncode)
    if expected_output.exists() and not diff_output(expected_output, program_output):
        raise RuntimeError("program output mismatch")

    return compile_ms, run_ms

def benchmark_suite(args, suite_path, output_root):
    cases = sorted(suite_path.glob("*.sy"))
    print(f"[suite] {suite_path} ({len(cases)} cases)")
    headers = ["run"] + [case.stem for case in cases]
    compile_rows = []
    runtime_rows = []
    work_dir = output_root / suite_path.name / "tmp"
    work_dir.mkdir(parents=True, exist_ok=True)
    run_prefix = f"{args.arch}_{normalize_flag(args.opt)}"

    for round_index in range(args.rounds):
        print(f"[round] {suite_path.name} round_{round_index + 1}")
        compile_row = [f"{run_prefix}_round_{round_index + 1}"]
        runtime_row = [f"{run_prefix}_round_{round_index + 1}"]
        for case_path in cases:
            try:
                compile_ms, run_ms = benchmark_case(
                    compiler=args.compiler,
                    case_path=case_path,
                    stage_flag="-llvm",
                    opt_flag=args.opt,
                    arch=args.arch,
                    libs_dir=args.libs_dir,
                    work_dir=work_dir,
                    timeout_compile=args.compile_timeout,
                    timeout_run=args.run_timeout,
                )
                compile_row.append(f"{compile_ms:.3f}")
                runtime_row.append(f"{run_ms:.3f}")
            except RuntimeError as error:
                print(f"[error] {case_path.name}: {error}")
                compile_row.append("ERR")
                runtime_row.append("ERR")
        compile_rows.append(compile_row)
        runtime_rows.append(runtime_row)

    suite_output_dir = output_root / suite_path.name
    suite_output_dir.mkdir(parents=True, exist_ok=True)
    compile_csv = suite_output_dir / "compile_time.csv"
    runtime_csv = suite_output_dir / "runtime_time.csv"
    compile_md = suite_output_dir / "compile_time.md"
    runtime_md = suite_output_dir / "runtime_time.md"

    merged_compile_rows = merge_rows(compile_csv, headers, compile_rows)
    merged_runtime_rows = merge_rows(runtime_csv, headers, runtime_rows)
    write_table(compile_csv, headers, merged_compile_rows)
    write_table(runtime_csv, headers, merged_runtime_rows)
    write_markdown_table(compile_md, headers, merged_compile_rows, f"{suite_path.name} compile time (ms)")
    write_markdown_table(runtime_md, headers, merged_runtime_rows, f"{suite_path.name} runtime time (ms)")

    if args.keep_tmp:
        return
    shutil.rmtree(work_dir, ignore_errors=True)

def parse_args():
    parser = argparse.ArgumentParser()
    script_dir = Path(__file__).resolve().parent
    repo_root = script_dir.parent.parent
    parser.add_argument("suites", nargs="+", help="suite directories that contain .sy files")
    parser.add_argument("--rounds", type=int, default=5)
    parser.add_argument("--arch", default="x86", choices=["x86", "arm", "rv"])
    parser.add_argument("--opt", default="-O0")
    parser.add_argument("--compile-timeout", type=int, default=30)
    parser.add_argument("--run-timeout", type=int, default=90)
    parser.add_argument("--keep-tmp", action="store_true")
    parser.add_argument("--output-root", type=Path, default=repo_root / "testoutputs" / "benchmark_llvm")
    parser.add_argument("--compiler", type=Path, default=repo_root / "bin" / "compiler")
    parser.add_argument("--libs-dir", type=Path, default=repo_root / "libs")
    return parser.parse_args()

def main():
    args = parse_args()
    output_root = Path(args.output_root)
    output_root.mkdir(parents=True, exist_ok=True)

    for suite in args.suites:
        suite_path = Path(suite)
        benchmark_suite(args, suite_path, output_root)

if __name__ == "__main__":
    main()
