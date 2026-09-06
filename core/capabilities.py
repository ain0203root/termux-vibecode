#!/usr/bin/env python3
from __future__ import annotations

import os
import platform
import subprocess
from pathlib import Path


def read_prop(name: str) -> str | None:
    try:
        return subprocess.check_output(["getprop", name], text=True, stderr=subprocess.DEVNULL, timeout=2).strip() or None
    except (FileNotFoundError, subprocess.SubprocessError):
        return None


def root_available() -> bool:
    try:
        return os.geteuid() == 0 or subprocess.run(["su", "-c", "id"], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, timeout=2).returncode == 0
    except (FileNotFoundError, subprocess.SubprocessError):
        return False


def thermal_zones() -> list[dict[str, str | int]]:
    result: list[dict[str, str | int]] = []
    root = Path("/sys/class/thermal")
    if not root.exists():
        return result
    for zone in sorted(root.glob("thermal_zone*")):
        try:
            typ = (zone / "type").read_text(encoding="utf-8").strip()
            raw = int((zone / "temp").read_text(encoding="utf-8").strip())
            result.append({"zone": zone.name, "type": typ, "temp_mC": raw})
        except (OSError, ValueError):
            continue
    return result


def snapshot() -> dict[str, object]:
    return {
        "os": platform.system(),
        "kernel": platform.release(),
        "machine": platform.machine(),
        "python": platform.python_version(),
        "termux": os.getenv("PREFIX", "").startswith("/data/data/com.termux"),
        "termux_prefix": os.getenv("PREFIX", ""),
        "android_release": read_prop("ro.build.version.release"),
        "android_api": read_prop("ro.build.version.sdk"),
        "android_abi": read_prop("ro.product.cpu.abi"),
        "android_soc": read_prop("ro.soc.model") or read_prop("ro.product.board"),
        "root_available": root_available(),
        "cpu_count": os.cpu_count(),
        "thermal_zones": thermal_zones(),
    }


if __name__ == "__main__":
    import json
    print(json.dumps(snapshot(), indent=2, ensure_ascii=False))
