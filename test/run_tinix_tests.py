#!/usr/bin/env python3
from __future__ import annotations

import argparse
import os
import re
import subprocess
import tempfile
from dataclasses import dataclass
from pathlib import Path


@dataclass(frozen=True)
class RunResult:
    code: int
    out: str
    err: str


def _load_swap_start(repo: Path) -> int:
    cfg = repo / "include" / "common" / "config.h"
    text = cfg.read_text(encoding="utf-8")

    def get_int(name: str) -> int:
        m = re.search(
            rf"constexpr\s+size_t\s+{re.escape(name)}\s*=\s*([0-9]+|0x[0-9A-Fa-f]+)\s*;",
            text,
        )
        if not m:
            raise RuntimeError(f"cannot find {name} in {cfg}")
        return int(m.group(1), 0)

    disk_blocks = get_int("DISK_NUM_BLOCKS")
    swap_reserved = get_int("SWAP_RESERVED_BLOCKS")
    return disk_blocks - swap_reserved


def _load_disk_params(repo: Path) -> tuple[int, int]:
    cfg = repo / "include" / "common" / "config.h"
    text = cfg.read_text(encoding="utf-8")

    def get_int(name: str) -> int:
        m = re.search(
            rf"constexpr\s+size_t\s+{re.escape(name)}\s*=\s*([0-9]+|0x[0-9A-Fa-f]+)\s*;",
            text,
        )
        if not m:
            raise RuntimeError(f"cannot find {name} in {cfg}")
        return int(m.group(1), 0)

    return get_int("DISK_NUM_BLOCKS"), get_int("DISK_BLOCK_SIZE")


def _load_fs_magic(repo: Path) -> int:
    defs = repo / "include" / "fs" / "fs_defs.h"
    text = defs.read_text(encoding="utf-8")
    m = re.search(r"constexpr\s+uint32_t\s+FS_MAGIC\s*=\s*(0x[0-9A-Fa-f]+)\s*;", text)
    if not m:
        raise RuntimeError(f"cannot find FS_MAGIC in {defs}")
    return int(m.group(1), 0)


def _run(exe: Path, commands: str, cwd: Path) -> RunResult:
    p = subprocess.run(
        [str(exe)],
        input=commands,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        cwd=str(cwd),
        timeout=10,
        env={**os.environ, "LC_ALL": "C"},
    )
    return RunResult(code=p.returncode, out=p.stdout, err=p.stderr)


def _require_contains(out: str, needle: str) -> None:
    if needle not in out:
        raise AssertionError(f"missing substring: {needle}\n--- output ---\n{out}")


def _swap_blocks(out: str) -> list[int]:
    blocks: list[int] = []
    for line in out.splitlines():
        if "[Swap]" not in line:
            continue
        m = re.search(r"Disk Block\s+(\d+)", line)
        if m:
            blocks.append(int(m.group(1)))
    return blocks


def _swap_write_blocks(err: str) -> list[int]:
    blocks: list[int] = []
    for line in err.splitlines():
        if "[Swap] Writing" not in line:
            continue
        m = re.search(r"Disk Block\s+(\d+)", line)
        if m:
            blocks.append(int(m.group(1)))
    return blocks


def _swap_read_blocks(err: str) -> list[int]:
    blocks: list[int] = []
    for line in err.splitlines():
        if "[Swap] Reading" not in line:
            continue
        m = re.search(r"Disk Block\s+(\d+)", line)
        if m:
            blocks.append(int(m.group(1)))
    return blocks


def _require_lines_regex(out: str, patterns: list[str]) -> None:
    lines = out.splitlines()
    if len(lines) < len(patterns):
        raise AssertionError(f"unexpected stdout\n--- stdout ---\n{out}")
    for i, pat in enumerate(patterns):
        if not re.fullmatch(pat, lines[i]):
            raise AssertionError(
                f"stdout line {i} mismatch: expected {pat!r}, got {lines[i]!r}\n--- stdout ---\n{out}"
            )
    for ln in lines[len(patterns) :]:
        if ln != "":
            raise AssertionError(f"unexpected stdout\n--- stdout ---\n{out}")


def case_shell_help(exe: Path, repo: Path) -> None:
    with tempfile.TemporaryDirectory() as td:
        cwd = Path(td)
        r = _run(exe, "help\nexit\n", cwd)
        if r.code != 0:
            raise AssertionError(r.out + r.err)
        _require_contains(r.out, "Available commands:")
        _require_contains(r.out, "=== File System Commands ===")
        _require_contains(r.err, "Tinix OS Shell")
        if "tinix>" in r.out:
            raise AssertionError(f"stdout contains prompt\n--- stdout ---\n{r.out}\n--- stderr ---\n{r.err}")


def case_fs_persistence(exe: Path, repo: Path) -> None:
    with tempfile.TemporaryDirectory() as td:
        cwd = Path(td)

        r1 = _run(
            exe,
            "\n".join(
                [
                    "format",
                    "mount",
                    "mkdir /a",
                    "cd /a",
                    "touch f",
                    "echo hello > f",
                    "exit",
                    "",
                ]
            ),
            cwd,
        )
        if r1.code != 0:
            raise AssertionError(r1.out + r1.err)

        r2 = _run(exe, "mount\ncat /a/f\nexit\n", cwd)
        if r2.code != 0:
            raise AssertionError(r2.out + r2.err)
        if r2.out.strip() != "hello":
            raise AssertionError(f"unexpected stdout\n--- stdout ---\n{r2.out}\n--- stderr ---\n{r2.err}")


def case_fs_paths(exe: Path, repo: Path) -> None:
    with tempfile.TemporaryDirectory() as td:
        cwd = Path(td)

        r = _run(
            exe,
            "\n".join(
                [
                    "format",
                    "mount",
                    "mkdir /a",
                    "mkdir /a/b",
                    "cd /a/b",
                    "pwd",
                    "cd ..",
                    "pwd",
                    "cd .",
                    "pwd",
                    "exit",
                    "",
                ]
            ),
            cwd,
        )
        if r.code != 0:
            raise AssertionError(r.out + r.err)
        expected = "/a/b\n/a\n/a\n"
        if r.out != expected:
            raise AssertionError(f"unexpected stdout\n--- stdout ---\n{r.out}\n--- stderr ---\n{r.err}")


def case_fs_rm_recreate(exe: Path, repo: Path) -> None:
    with tempfile.TemporaryDirectory() as td:
        cwd = Path(td)

        r = _run(
            exe,
            "\n".join(
                [
                    "format",
                    "mount",
                    "touch a",
                    "echo one > a",
                    "rm a",
                    "touch a",
                    "echo two > a",
                    "cat a",
                    "exit",
                    "",
                ]
            ),
            cwd,
        )
        if r.code != 0:
            raise AssertionError(r.out + r.err)
        if r.out.strip() != "two":
            raise AssertionError(f"unexpected stdout\n--- stdout ---\n{r.out}\n--- stderr ---\n{r.err}")


def case_shell_script(exe: Path, repo: Path) -> None:
    with tempfile.TemporaryDirectory() as td:
        cwd = Path(td)

        script = cwd / "t.tsh"
        script.write_text(
            "\n".join(
                [
                    "format",
                    "mount",
                    "mkdir /s",
                    "cd /s",
                    "touch f",
                    "echo hi > f",
                    "cat f",
                ]
            )
            + "\n",
            encoding="utf-8",
        )

        r = _run(exe, f"script {script.name}\nexit\n", cwd)
        if r.code != 0:
            raise AssertionError(r.out + r.err)
        if r.out.strip() != "hi":
            raise AssertionError(f"unexpected stdout\n--- stdout ---\n{r.out}\n--- stderr ---\n{r.err}")


def case_complex_fs_swap_roundtrip(exe: Path, repo: Path) -> None:
    swap_start = _load_swap_start(repo)

    with tempfile.TemporaryDirectory() as td:
        cwd = Path(td)

        pc = cwd / "mm_cycle.pc"
        lines = ["# swap write+read"]
        for i in range(0, 9):
            lines.append(f"W {i * 0x1000}")
        lines.append("W 0")
        pc.write_text("\n".join(lines) + "\n", encoding="utf-8")

        r = _run(
            exe,
            "\n".join(
                [
                    "format",
                    "mount",
                    "mkdir /t",
                    "cd /t",
                    "touch keep",
                    "echo keepme > keep",
                    f"create -f {pc.name}",
                    "tick 30",
                    "pwd",
                    "ls .",
                    "cat keep",
                    "exit",
                    "",
                ]
            ),
            cwd,
        )
        if r.code != 0:
            raise AssertionError(r.out + r.err)

        _require_lines_regex(
            r.out,
            [
                r"/t",
                r"Contents of \.:",
                r"  d \. \(inode=\d+, size=96\)",
                r"  d \.\. \(inode=\d+, size=96\)",
                r"  - keep \(inode=\d+, size=7\)",
                r"keepme",
            ],
        )

        write_blocks = _swap_write_blocks(r.err)
        read_blocks = _swap_read_blocks(r.err)
        if not write_blocks or not read_blocks:
            raise AssertionError(f"expected swap write+read\n--- stderr ---\n{r.err}")
        if min(write_blocks + read_blocks) < swap_start:
            raise AssertionError(f"swap used FS region\n--- stderr ---\n{r.err}")
        if not (set(write_blocks) & set(read_blocks)):
            raise AssertionError(f"expected swap block roundtrip\n--- stderr ---\n{r.err}")
        if not re.search(r"\[Tick\] Process \d+ completed", r.err):
            raise AssertionError(f"expected process completion\n--- stderr ---\n{r.err}")


def case_complex_two_proc_cross_evict(exe: Path, repo: Path) -> None:
    swap_start = _load_swap_start(repo)

    with tempfile.TemporaryDirectory() as td:
        cwd = Path(td)

        p1 = cwd / "p1.pc"
        p2 = cwd / "p2.pc"

        p1_lines = ["# p1", *[f"W {i * 0x1000}" for i in range(0, 9)], "S 3", "W 0"]
        p2_lines = ["# p2", *[f"W {i * 0x1000}" for i in range(0, 9)], "W 0"]
        p1.write_text("\n".join(p1_lines) + "\n", encoding="utf-8")
        p2.write_text("\n".join(p2_lines) + "\n", encoding="utf-8")

        r = _run(
            exe,
            "\n".join(
                [
                    "format",
                    "mount",
                    "mkdir /w",
                    "cd /w",
                    "touch msg",
                    "echo ok > msg",
                    f"create -f {p1.name}",
                    f"create -f {p2.name}",
                    "tick 80",
                    "pwd",
                    "ls .",
                    "cat msg",
                    "exit",
                    "",
                ]
            ),
            cwd,
        )
        if r.code != 0:
            raise AssertionError(r.out + r.err)

        _require_lines_regex(
            r.out,
            [
                r"/w",
                r"Contents of \.:",
                r"  d \. \(inode=\d+, size=96\)",
                r"  d \.\. \(inode=\d+, size=96\)",
                r"  - msg \(inode=\d+, size=3\)",
                r"ok",
            ],
        )

        m1 = re.search(rf"Created process PID:\s*(\d+)\s+from {re.escape(p1.name)}", r.err)
        m2 = re.search(rf"Created process PID:\s*(\d+)\s+from {re.escape(p2.name)}", r.err)
        if not m1 or not m2:
            raise AssertionError(f"missing pids\n--- stderr ---\n{r.err}")
        pid1 = int(m1.group(1))
        pid2 = int(m2.group(1))

        for pid in (pid1, pid2):
            if not re.search(rf"\[PageFault\] PID={pid}\b", r.err):
                raise AssertionError(f"missing page faults for pid {pid}\n--- stderr ---\n{r.err}")
            if not re.search(rf"\[Tick\] Process {pid} completed\b", r.err):
                raise AssertionError(f"missing completion for pid {pid}\n--- stderr ---\n{r.err}")

        if not re.search(rf"\[Tick\] Process {pid1} auto-woken up\b", r.err):
            raise AssertionError(f"missing auto-wakeup for pid {pid1}\n--- stderr ---\n{r.err}")

        blocks = _swap_blocks(r.err)
        if blocks and min(blocks) < swap_start:
            raise AssertionError(f"swap used FS region\n--- stderr ---\n{r.err}")

        lines = r.err.splitlines()
        cross = False
        for i, line in enumerate(lines):
            m = re.search(r"\[Evict\] Replacing Frame \d+ from PID=(\d+), VPage=(\d+)", line)
            if not m:
                continue
            victim_pid = int(m.group(1))
            for j in range(i + 1, min(i + 15, len(lines))):
                m2 = re.search(r"\[PageFault\] Allocated Frame \d+ for PID=(\d+), VPage=(\d+)", lines[j])
                if not m2:
                    continue
                alloc_pid = int(m2.group(1))
                if alloc_pid != victim_pid and {alloc_pid, victim_pid} <= {pid1, pid2}:
                    cross = True
                break
            if cross:
                break
        if not cross:
            raise AssertionError(f"expected cross-pid eviction\n--- stderr ---\n{r.err}")


def case_swap_partition(exe: Path, repo: Path) -> None:
    swap_start = _load_swap_start(repo)

    with tempfile.TemporaryDirectory() as td:
        cwd = Path(td)

        pc = cwd / "mm.pc"
        lines = ["# many writes to trigger swap"]
        for i in range(0, 32):
            lines.append(f"W {i * 0x1000}")
        pc.write_text("\n".join(lines) + "\n", encoding="utf-8")

        r1 = _run(
            exe,
            "\n".join(
                [
                    "format",
                    "mount",
                    "touch keep",
                    "echo keepme > keep",
                    f"create -f {pc.name}",
                    "tick 120",
                    "exit",
                    "",
                ]
            ),
            cwd,
        )
        if r1.code != 0:
            raise AssertionError(r1.out + r1.err)

        blocks = _swap_blocks(r1.err)
        if not blocks:
            raise AssertionError(f"expected swap activity\n--- stdout ---\n{r1.out}\n--- stderr ---\n{r1.err}")
        if min(blocks) < swap_start:
            raise AssertionError(
                f"swap wrote into FS region: min={min(blocks)} swap_start={swap_start}\n--- stdout ---\n{r1.out}\n--- stderr ---\n{r1.err}"
            )

        r2 = _run(exe, "mount\ncat /keep\nexit\n", cwd)
        if r2.code != 0:
            raise AssertionError(r2.out + r2.err)
        if r2.out.strip() != "keepme":
            raise AssertionError(f"unexpected stdout\n--- stdout ---\n{r2.out}\n--- stderr ---\n{r2.err}")


def case_kernel_reformat_mismatch(exe: Path, repo: Path) -> None:
    disk_blocks, block_size = _load_disk_params(repo)
    fs_magic = _load_fs_magic(repo)

    with tempfile.TemporaryDirectory() as td:
        cwd = Path(td)

        disk = cwd / "disk.img"
        disk.write_bytes(b"\x00" * (disk_blocks * block_size))

        # superblock: magic + total_blocks + total_inodes + free_blocks + free_inodes
        sb = bytearray(block_size)
        sb[0:4] = int(fs_magic).to_bytes(4, "little", signed=False)
        sb[4:8] = int(disk_blocks).to_bytes(4, "little", signed=False)  # wrong after partition
        sb[8:12] = int(128).to_bytes(4, "little", signed=False)
        with disk.open("r+b") as f:
            f.seek(0)
            f.write(sb)

        r = _run(exe, "mount\nfsinfo\nexit\n", cwd)
        if r.code != 0:
            raise AssertionError(r.out + r.err)
        if r.out.strip() != "":
            raise AssertionError(f"unexpected stdout\n--- stdout ---\n{r.out}\n--- stderr ---\n{r.err}")
        _require_contains(r.err, "layout mismatch, please re-format")
        _require_contains(r.err, "[Kernel] File system not found, formatting...")
        _require_contains(r.err, "Mount successful!")


CASES = {
    "shell_help": case_shell_help,
    "fs_persistence": case_fs_persistence,
    "fs_paths": case_fs_paths,
    "fs_rm_recreate": case_fs_rm_recreate,
    "shell_script": case_shell_script,
    "complex_fs_swap_roundtrip": case_complex_fs_swap_roundtrip,
    "complex_two_proc_cross_evict": case_complex_two_proc_cross_evict,
    "swap_partition": case_swap_partition,
    "kernel_reformat_mismatch": case_kernel_reformat_mismatch,
}


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--exe", required=True, type=Path)
    ap.add_argument("--repo", required=True, type=Path)
    ap.add_argument("--case", required=True, choices=sorted(CASES.keys()))
    args = ap.parse_args()

    CASES[args.case](args.exe, args.repo)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
