#!/usr/bin/env python3
from __future__ import annotations

import json
import os
import shutil
import subprocess
import sys
import time
from pathlib import Path

HOME = Path.home() / ".vibecode"
LOG = HOME / "logs" / "events.jsonl"

# The installed copy lives beside capabilities.py under ~/.vibecode.
HERE = Path(__file__).resolve().parent
if str(HERE) not in sys.path:
    sys.path.insert(0, str(HERE))
try:
    from capabilities import snapshot as capability_snapshot
except ImportError:
    capability_snapshot = None


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
    data: dict[str, object] = {
        "platform": os.uname().sysname if hasattr(os, "uname") else "unknown",
        "kernel": os.uname().release if hasattr(os, "uname") else None,
        "machine": os.uname().machine if hasattr(os, "uname") else None,
        "python": sys.version.split()[0],
        "termux_prefix": os.getenv("PREFIX", ""),
        "home": str(Path.home()),
        "free_gib": round(free / 1024**3, 2),
        "used_gib": round(used / 1024**3, 2),
        "total_gib": round(total / 1024**3, 2),
        "cpu_count": os.cpu_count(),
    }
    rc, load = run(["cat", "/proc/loadavg"])
    data["loadavg"] = load if rc == 0 else None
    if capability_snapshot is not None:
        data["android"] = capability_snapshot()
    emit("status", **data)
    return data


def doctor() -> dict[str, object]:
    required_names = ["bash", "python"]
    optional_names = ["clang", "make", "git", "ssh", "tmux"]
    required = {name: shutil.which(name) or False for name in required_names}
    optional = {name: shutil.which(name) or False for name in optional_names}
    required["termux"] = os.getenv("PREFIX", "").startswith("/data/data/com.termux")
    required["procfs"] = Path("/proc").exists()
    required["home"] = Path.home().exists()
    missing_required = [k for k, v in required.items() if not v]
    missing_optional = [k for k, v in optional.items() if not v]
    result: dict[str, object] = {
        "ok": not missing_required,
        "required": required,
        "optional": optional,
        "missing_required": missing_required,
        "missing_optional": missing_optional,
    }
    if capability_snapshot is not None:
        result["capabilities"] = capability_snapshot()
    emit("doctor", **result)
    return result


def main() -> None:
    op = sys.argv[1] if len(sys.argv) > 1 else "status"
    result = status() if op == "status" else doctor() if op == "doctor" else {"ok": False, "error": f"unknown op {op}"}
    print(json.dumps(result, indent=2, ensure_ascii=False))
    raise SystemExit(0 if result.get("ok", True) else 1)


if __name__ == "__main__":
    main()
