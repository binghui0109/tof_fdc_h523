#!/usr/bin/env python3
"""
Helper script to run clang-tidy and cppcheck with optional vendor filtering.
"""
from __future__ import annotations

import argparse
import os
import shutil
import subprocess
import sys
from pathlib import Path
import json
import shlex


REPO_ROOT = Path(__file__).resolve().parents[1]


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Run clang-tidy and cppcheck using compile_commands.json."
    )
    parser.add_argument(
        "--compile-db",
        default="compile_commands.json",
        help="Path to compile_commands.json (relative to repo root).",
    )
    parser.add_argument(
        "--with-vendor",
        action="store_true",
        help="Include vendor-generated sources. Off by default.",
    )
    parser.add_argument(
        "--jobs",
        type=int,
        default=max(1, (os.cpu_count() or 2)),
        help="Parallel jobs for run-clang-tidy.",
    )
    parser.add_argument(
        "--clang-tidy-runner",
        default=shutil.which("run-clang-tidy-17") or shutil.which("run-clang-tidy"),
        help="Path to run-clang-tidy runner script.",
    )
    parser.add_argument(
        "--clang-tidy-bin",
        default=shutil.which("clang-tidy-17") or shutil.which("clang-tidy"),
        help="clang-tidy binary to invoke.",
    )
    parser.add_argument(
        "--dump-clang-tidy-config",
        action="store_true",
        help="Write .clang-tidy based on --clang-tidy-checks and exit.",
    )
    parser.add_argument(
        "--clang-tidy-checks",
        default="bugprone-*,modernize-*,performance-*,readability-*",
        help="Checks string for clang-tidy when dumping config.",
    )
    return parser.parse_args()


def ensure_prereqs(args: argparse.Namespace, compile_db: Path) -> None:
    if args.clang_tidy_runner is None:
        sys.exit("run-clang-tidy binary not found (expected clang-tools-17).")
    if args.clang_tidy_bin is None:
        sys.exit("clang-tidy binary not found (expected clang-tidy-17).")
    if shutil.which("cppcheck") is None:
        sys.exit("cppcheck not found. Install it (e.g. sudo apt install cppcheck).")
    if not compile_db.exists():
        sys.exit(
            f"Compilation database '{compile_db}' missing. "
            "Build at least one board to regenerate it."
        )


def dump_clang_tidy_config(args: argparse.Namespace, output_path: Path) -> int:
    if args.clang_tidy_bin is None:
        sys.exit("clang-tidy binary not found (expected clang-tidy-17).")
    cmd = [
        args.clang_tidy_bin,
        f"-checks={args.clang_tidy_checks}",
        "--dump-config",
    ]
    print("Running:", " ".join(cmd), flush=True)
    completed = subprocess.run(cmd, cwd=REPO_ROOT, capture_output=True, text=True)
    if completed.returncode != 0:
        sys.stderr.write(completed.stderr)
        return completed.returncode
    output_path.write_text(completed.stdout, encoding="utf-8")
    return 0


def discover_vendor_dirs() -> list[Path]:
    vendor_dirs: set[Path] = set()
    keywords = ("vendor", "cmsis")
    for path in REPO_ROOT.rglob("*"):
        if not path.is_dir():
            continue
        name = path.name.lower()
        if any(key in name for key in keywords):
            vendor_dirs.add(path.relative_to(REPO_ROOT))
    return sorted(vendor_dirs)


def vendor_regex(include_vendor: bool) -> str:
    if include_vendor:
        return r".*"
    return r"^(?!.*([Vv]endor|[Cc][Mm][Ss][Ii][Ss])).*$"


def run_clang_tidy(args: argparse.Namespace, compile_db: Path, include_vendor: bool) -> int:
    files_regex = vendor_regex(include_vendor)
    header_regex = files_regex
    toolchain = REPO_ROOT / "tools" / "gcc"
    cmd = [
        args.clang_tidy_runner,
        "-clang-tidy-binary",
        args.clang_tidy_bin,
        "-p",
        str(REPO_ROOT),
        "-header-filter",
        header_regex,
        "-j",
        str(args.jobs),
        "-warnings-as-errors",
        "*",
        # "-quiet",
        "-use-color"
    ]
    cmd.extend(
        [
            f"-extra-arg=-Wno-ignored-optimization-argument"
        ]
    )
    if toolchain.exists():
        sysroot = toolchain / "arm-none-eabi"
        cmd.extend(
            [
                # f"-extra-arg-before=--gcc-toolchain={toolchain}",
                f"-extra-arg-before=--target=arm-none-eabi",
            ]
        )
        if sysroot.exists():
            cmd.append(f"-extra-arg-before=--sysroot={sysroot}")
    cmd.append(files_regex)
    print("Running:", " ".join(cmd), flush=True)
    completed = subprocess.run(cmd, cwd=REPO_ROOT)
    return completed.returncode


BASE_CPP_MACROS = {
    "__GNUC__": "10",
    "__GNUC_MINOR__": "3",
    "__GNUC_PATCHLEVEL__": "1",
}

ARCH_CPP_MACROS = {
    "cortex-m33": {
        "__ARM_ARCH": "8",
        "__ARM_ARCH_PROFILE": "'M'",
        "__ARM_ARCH_8M_MAIN__": "1",
        "__ARM_FEATURE_CMSE": "3",
    },
    "cortex-m23": {
        "__ARM_ARCH": "8",
        "__ARM_ARCH_PROFILE": "'M'",
        "__ARM_ARCH_8M_BASE__": "1",
        "__ARM_FEATURE_CMSE": "3",
    },
    "cortex-m4": {
        "__ARM_ARCH": "7",
        "__ARM_ARCH_PROFILE": "'M'",
        "__ARM_ARCH_7EM__": "1",
    },
    "cortex-m7": {
        "__ARM_ARCH": "7",
        "__ARM_ARCH_PROFILE": "'M'",
        "__ARM_ARCH_7EM__": "1",
    },
    "cortex-m3": {
        "__ARM_ARCH": "7",
        "__ARM_ARCH_PROFILE": "'M'",
        "__ARM_ARCH_7M__": "1",
    },
    "cortex-m0": {
        "__ARM_ARCH": "6",
        "__ARM_ARCH_PROFILE": "'M'",
        "__ARM_ARCH_6M__": "1",
    },
    "cortex-m0plus": {
        "__ARM_ARCH": "6",
        "__ARM_ARCH_PROFILE": "'M'",
        "__ARM_ARCH_6M__": "1",
    },
    "cortex-m55": {
        "__ARM_ARCH": "8",
        "__ARM_ARCH_PROFILE": "'M'",
        "__ARM_ARCH_8_1M_MAIN__": "1",
    },
    "cortex-m85": {
        "__ARM_ARCH": "8",
        "__ARM_ARCH_PROFILE": "'M'",
        "__ARM_ARCH_8_1M_MAIN__": "1",
    },
}


def iter_compile_args(compile_db: Path) -> list[list[str]]:
    try:
        entries = json.loads(compile_db.read_text())
    except (json.JSONDecodeError, OSError):
        return []
    args_list: list[list[str]] = []
    for entry in entries:
        args = entry.get("arguments")
        if isinstance(args, list) and args:
            args_list.append(args)
            continue
        command = entry.get("command")
        if isinstance(command, str) and command:
            try:
                args_list.append(shlex.split(command))
            except ValueError:
                continue
    return args_list


def detect_mcpu(compile_db: Path) -> str | None:
    for args in iter_compile_args(compile_db):
        for arg in args:
            if arg.startswith("-mcpu="):
                value = arg.split("=", 1)[1].lower()
                return value.split("+", 1)[0]
    return None


def build_cppcheck_defines(compile_db: Path) -> list[str]:
    defines = ["-U__clang__"]
    defines.extend(f"-D{k}={v}" for k, v in BASE_CPP_MACROS.items())
    mcpu = detect_mcpu(compile_db)
    if mcpu:
        arch_macros = ARCH_CPP_MACROS.get(mcpu)
        if arch_macros:
            defines.extend(f"-D{k}={v}" for k, v in arch_macros.items())
        else:
            print(f"Warning: Unhandled -mcpu setting '{mcpu}', using default cppcheck macros.")
    else:
        print("Warning: Unable to detect -mcpu flag from compile_commands.json; cppcheck may mis-detect CMSIS defines.")
    return defines


def run_cppcheck(
    compile_db: Path, include_vendor: bool, vendor_dirs: list[Path], extra_defines: list[str]
) -> int:
    cmd = [
        "cppcheck",
        f"--project={compile_db}",
        "--enable=warning,style,performance,portability",
        "--inline-suppr",
        "--suppress=missingIncludeSystem",
        "--suppressions-list=cppcheck_suppressions.txt",
    ]
    if not include_vendor:
        for directory in vendor_dirs:
            cmd.extend(["-i", str(directory)])
    cmd.extend(extra_defines)
    print("Running:", " ".join(cmd), flush=True)
    completed = subprocess.run(cmd, cwd=REPO_ROOT)
    return completed.returncode


def main() -> None:
    args = parse_args()
    compile_db = (REPO_ROOT / args.compile_db).resolve()
    if args.dump_clang_tidy_config:
        rc = dump_clang_tidy_config(args, REPO_ROOT / ".clang-tidy")
        if rc:
            sys.exit(rc)
        return
    ensure_prereqs(args, compile_db)
    vendor_dirs = discover_vendor_dirs()
    if not args.with_vendor:
        print(
            f"Vendor scanning disabled. Pass --with-vendor to include {len(vendor_dirs)} vendor directories.",
            flush=True,
        )
    extra_defines = build_cppcheck_defines(compile_db)
    tidy_rc = run_clang_tidy(args, compile_db, args.with_vendor)
    cpp_rc = run_cppcheck(compile_db, args.with_vendor, vendor_dirs, extra_defines)
    if tidy_rc or cpp_rc:
        sys.exit(tidy_rc or cpp_rc)


if __name__ == "__main__":
    main()
