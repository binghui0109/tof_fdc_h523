#!/usr/bin/env python3
import argparse
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile


def find_repo_root(script_path: Path) -> Path:
    return script_path.parent.parent


def read_version_from_cmake(cmake_path: Path) -> str:
    if not cmake_path.is_file():
        return ""
    text = cmake_path.read_text(encoding="utf-8", errors="ignore")
    match = re.search(r'set\s*\(\s*VERSION\s+"([^"]+)"\s*\)', text, re.IGNORECASE)
    return match.group(1) if match else ""


def git_short_hash(repo_root: Path) -> str:
    try:
        result = subprocess.run(
            ["git", "rev-parse", "--short", "HEAD"],
            cwd=str(repo_root),
            check=True,
            capture_output=True,
            text=True,
        )
    except (subprocess.SubprocessError, FileNotFoundError):
        return ""
    return result.stdout.strip()


def find_objcopy(repo_root: Path) -> str:
    candidates = [
        repo_root / "tools" / "gcc" / "bin" / "arm-none-eabi-objcopy",
        repo_root / "tools" / "gcc" / "bin" / "arm-none-eabi-objcopy.exe",
    ]
    for candidate in candidates:
        if candidate.is_file():
            return str(candidate)
    return ""


def find_firmware_files(build_root: Path, config: str) -> list[Path]:
    if not build_root.is_dir():
        return []
    firmware_files: list[Path] = []
    for ext in ("*.hex", "*.bin"):
        for fw_path in build_root.rglob(ext):
            if config in fw_path.parts:
                if "CMakeFiles" in fw_path.parts:
                    continue
                firmware_files.append(fw_path)
    return sorted(firmware_files)


def contains_bytes_in_hex(hex_path: Path, needle: bytes) -> bool:
    base_addr = 0
    segments: list[tuple[int, bytearray]] = []

    with hex_path.open("r", encoding="ascii", errors="ignore") as handle:
        for line in handle:
            line = line.strip()
            if not line.startswith(":") or len(line) < 11:
                continue
            try:
                byte_count = int(line[1:3], 16)
                addr = int(line[3:7], 16)
                record_type = int(line[7:9], 16)
            except ValueError:
                continue

            data_hex = line[9 : 9 + (byte_count * 2)]
            if len(data_hex) != byte_count * 2:
                continue
            try:
                data = bytes.fromhex(data_hex)
            except ValueError:
                continue

            if record_type == 0x00:
                full_addr = base_addr + addr
                if segments and segments[-1][0] + len(segments[-1][1]) == full_addr:
                    segments[-1][1].extend(data)
                else:
                    segments.append((full_addr, bytearray(data)))
            elif record_type == 0x04 and len(data) == 2:
                base_addr = int.from_bytes(data, "big") << 16
            elif record_type == 0x01:
                break

    for _, data in segments:
        if needle in data:
            return True
    return False


def hex_contains_string(hex_path: Path, expected: str, objcopy: str, repo_root: Path) -> bool:
    needle = expected.encode("ascii", errors="strict")

    if objcopy:
        with tempfile.TemporaryDirectory() as temp_dir:
            bin_path = Path(temp_dir) / f"{hex_path.stem}.bin"
            cmd = [objcopy, "-I", "ihex", "-O", "binary", str(hex_path), str(bin_path)]
            result = subprocess.run(cmd, cwd=str(repo_root), capture_output=True, text=True)
            if result.returncode == 0 and bin_path.is_file():
                data = bin_path.read_bytes()
                return needle in data

    return contains_bytes_in_hex(hex_path, needle)


def bin_contains_string(bin_path: Path, expected: str) -> bool:
    needle = expected.encode("ascii", errors="strict")
    return needle in bin_path.read_bytes()


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Verify that release firmware files contain the expected version/hash string.",
    )
    parser.add_argument(
        "--build-root",
        default="build",
        help="Build root directory to scan (default: build).",
    )
    parser.add_argument(
        "--config",
        default="Release",
        help="Build config name to match in paths (default: Release).",
    )
    parser.add_argument(
        "--expected",
        default="",
        help="Expected ASCII string to search for. Overrides version/hash detection.",
    )
    parser.add_argument(
        "--hash-only",
        action="store_true",
        help="Match only the git short hash (no version prefix).",
    )
    parser.add_argument(
        "--git-hash",
        default="",
        help="Override git short hash (default: current HEAD).",
    )
    parser.add_argument(
        "--cmake",
        default="CMakeLists.txt",
        help="Path to CMakeLists.txt for version parsing.",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    repo_root = find_repo_root(Path(__file__).resolve())
    build_root = repo_root / args.build_root
    cmake_path = repo_root / args.cmake

    if args.expected:
        expected = args.expected
    else:
        git_hash = args.git_hash or git_short_hash(repo_root)
        if not git_hash:
            print("ERROR: Unable to determine git hash.", file=sys.stderr)
            return 2
        if args.hash_only:
            expected = git_hash
        else:
            version = read_version_from_cmake(cmake_path)
            if not version:
                print("ERROR: Unable to determine version from CMakeLists.txt.", file=sys.stderr)
                return 2
            expected = f"{version}_{git_hash}"

    firmware_files = find_firmware_files(build_root, args.config)
    if not firmware_files:
        print(
            f"ERROR: No .hex or .bin files found under {build_root} for config {args.config}.",
            file=sys.stderr,
        )
        return 2

    objcopy = find_objcopy(repo_root)
    if not objcopy and any(path.suffix.lower() == ".hex" for path in firmware_files):
        print("WARN: arm-none-eabi-objcopy not found, using HEX parser fallback.", file=sys.stderr)

    print(f"Expected string: {expected}")
    missing = []
    for fw_path in firmware_files:
        if fw_path.suffix.lower() == ".bin":
            ok = bin_contains_string(fw_path, expected)
        else:
            ok = hex_contains_string(fw_path, expected, objcopy, repo_root)
        status = "OK" if ok else "MISSING"
        print(f"{status}: {fw_path}")
        if not ok:
            missing.append(fw_path)

    return 1 if missing else 0


if __name__ == "__main__":
    raise SystemExit(main())
