#!/usr/bin/env python3
from __future__ import annotations

import json
import os
import platform
import shutil
import subprocess
import time
from pathlib import Path

HOME = Path.home() / ".vibecode"
LOG = HOME / "logs" / "events.jsonl"


def emit(kind: str, **data: object) -> None:
    LOG.parent.mkdir(parents=True, exist_ok=True)
    record = json.dumps(
        {"ts": time.time(), "kind": kind, **data},
        sort_keys=True,
        separators=(",", ":"),
        ensure_ascii=False,
    ) + "\n"
    fd = os.open(LOG, os.O_WRONLY | os.O_CREAT | os.O_APPEND, 0o644)
    try:
        os.write(fd, record.encode("utf-8"))
    finally:
        os.close(fd)


def run(args: list[str]) -> tuple[int, str]:
    try:
        p = subprocess.run(
            args,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            timeout=8,
            check=False,
        )
        return p.returncode, p.stdout.strip()
    except Exception as exc:
        return 127, str(exc)


def status() -> dict[str, object]:
    total, used, free = shutil.disk_usage(Path.home())
    data = {
        "platform": platform.platform(),
        "machine": platform.machine(),
        "python": platform.python_version(),
        "termux_prefix": os.getenv("PREFIX", ""),
        "home": str(Path.home()),
        "free_gib": round(free / 1024**3, 2),
        "used_gib": round(used / 1024**3, 2),
        "total_gib": round(total / 1024**3, 2),
        "cpu_count": os.cpu_count(),
    }
    rc, load = run(["cat", "/proc/loadavg"])
    data["loadavg"] = load if rc == 0 else None
    emit("status", **data)
    return data


def doctor() -> dict[str, object]:
    names = ["bash", "clang", "make", "python", "git", "ssh", "tmux"]
    checks = {name: shutil.which(name) or False for name in names}
    checks["termux"] = os.getenv("PREFIX", "").startswith("/data/data/com.termux")
    checks["procfs"] = Path("/proc").exists()
    checks["home"] = Path.home().exists()
    missing = [k for k, v in checks.items() if not v]
    result = {"ok": not missing, "checks": checks, "missing": missing}
    emit("doctor", **result)
    return result


def main() -> None:
    import sys

    op = sys.argv[1] if len(sys.argv) > 1 else "status"
    result = status() if op == "status" else doctor() if op == "doctor" else {"ok": False, "error": f"unknown op {op}"}
    print(json.dumps(result, indent=2, ensure_ascii=False))
    raise SystemExit(0 if result.get("ok", True) else 1)


if __name__ == "__main__":
    main()
