#!/usr/bin/env python3
from __future__ import annotations

import json
import os
import re
import signal
import subprocess
import time
from dataclasses import dataclass
from pathlib import Path

HOME = Path.home() / ".vibecode"
CONF = HOME / "services.conf"
RUN = HOME / "run"
LOG = HOME / "logs" / "services"
NAME_RE = re.compile(r"^[A-Za-z0-9._-]+$")


@dataclass(frozen=True)
class Service:
    name: str
    command: str
    restart: bool = True
    delay: float = 1.0
    max_delay: float = 30.0


def load() -> list[Service]:
    out: list[Service] = []
    seen: set[str] = set()
    if not CONF.exists():
        return out
    for raw in CONF.read_text(encoding="utf-8").splitlines():
        line = raw.strip()
        if not line or line.startswith("#") or "=" not in line:
            continue
        name, command = line.split("=", 1)
        name, command = name.strip(), command.strip()
        if not name or not command or not NAME_RE.fullmatch(name) or name in seen:
            continue
        seen.add(name)
        out.append(Service(name, command))
    return out


def path_for(name: str) -> Path:
    if not NAME_RE.fullmatch(name):
        raise ValueError(f"invalid service name: {name}")
    return RUN / f"{name}.pid"


def _proc_starttime(pid: int) -> int | None:
    try:
        stat = Path(f"/proc/{pid}/stat").read_text(encoding="ascii")
        close = stat.rfind(")")
        if close < 0:
            return None
        fields = stat[close + 2 :].split()
        return int(fields[19])
    except (OSError, ValueError, IndexError):
        return None


def _proc_state(pid: int) -> str | None:
    try:
        stat = Path(f"/proc/{pid}/stat").read_text(encoding="ascii")
        close = stat.rfind(")")
        if close < 0:
            return None
        fields = stat[close + 2 :].split()
        return fields[0] if fields else None
    except OSError:
        return None


def _read_record(path: Path) -> tuple[int, int | None] | None:
    try:
        raw = path.read_text(encoding="utf-8").strip()
        if raw.startswith("{"):
            data = json.loads(raw)
            return int(data["pid"]), int(data["starttime"]) if data.get("starttime") is not None else None
        return int(raw), None
    except (OSError, ValueError, TypeError, KeyError, json.JSONDecodeError):
        return None


def _remove_stale(path: Path, pid: int) -> None:
    try:
        os.waitpid(pid, os.WNOHANG)
    except (ChildProcessError, PermissionError, ProcessLookupError):
        pass
    path.unlink(missing_ok=True)


def running(name: str) -> int | None:
    path = path_for(name)
    record = _read_record(path) if path.exists() else None
    if record is None:
        path.unlink(missing_ok=True)
        return None
    pid, expected_start = record
    try:
        os.kill(pid, 0)
    except (ProcessLookupError, PermissionError):
        path.unlink(missing_ok=True)
        return None
    if expected_start is not None and _proc_starttime(pid) != expected_start:
        path.unlink(missing_ok=True)
        return None
    if _proc_state(pid) == "Z":
        _remove_stale(path, pid)
        return None
    return pid


def _write_record(service: Service, pid: int) -> None:
    RUN.mkdir(parents=True, exist_ok=True)
    path = path_for(service.name)
    tmp = path.with_suffix(path.suffix + f".tmp.{os.getpid()}")
    record = {
        "pid": pid,
        "starttime": _proc_starttime(pid),
        "command": service.command,
    }
    tmp.write_text(json.dumps(record, sort_keys=True) + "\n", encoding="utf-8")
    os.replace(tmp, path)


def start(service: Service) -> int:
    existing = running(service.name)
    if existing:
        return existing
    log_dir = LOG / service.name
    log_dir.mkdir(parents=True, exist_ok=True)
    with (log_dir / "stdout.log").open("ab") as log:
        proc = subprocess.Popen(
            service.command,
            shell=True,
            stdout=log,
            stderr=subprocess.STDOUT,
            start_new_session=True,
            close_fds=True,
        )
    _write_record(service, proc.pid)
    (log_dir / "started_at").write_text(str(time.time()), encoding="ascii")
    return proc.pid


def stop(service: Service) -> None:
    pid = running(service.name)
    if pid is None:
        return
    try:
        os.killpg(pid, signal.SIGTERM)
    except (ProcessLookupError, PermissionError):
        try:
            os.kill(pid, signal.SIGTERM)
        except ProcessLookupError:
            pass
    deadline = time.monotonic() + 3.0
    while time.monotonic() < deadline and running(service.name) is not None:
        time.sleep(0.05)
    pid = running(service.name)
    if pid is not None:
        try:
            os.killpg(pid, signal.SIGKILL)
        except (ProcessLookupError, PermissionError):
            pass
    path_for(service.name).unlink(missing_ok=True)


def snapshot() -> list[dict[str, object]]:
    return [
        {"name": s.name, "command": s.command, "restart": s.restart, "pid": running(s.name)}
        for s in load()
    ]


def supervise() -> None:
    services = load()
    backoff = {s.name: s.delay for s in services}
    next_start = {s.name: 0.0 for s in services}
    for service in services:
        start(service)
        next_start[service.name] = time.monotonic() + service.delay
    while True:
        now = time.monotonic()
        for service in services:
            if running(service.name) is not None:
                backoff[service.name] = service.delay
                next_start[service.name] = now + service.delay
                continue
            if not service.restart or now < next_start[service.name]:
                continue
            current_delay = backoff[service.name]
            start(service)
            next_start[service.name] = now + min(current_delay * 2.0, service.max_delay)
            backoff[service.name] = min(current_delay * 2.0, service.max_delay)
        time.sleep(0.5)


if __name__ == "__main__":
    import sys

    op = sys.argv[1] if len(sys.argv) > 1 else "status"
    services = {s.name: s for s in load()}
    if op == "status":
        print(json.dumps(snapshot(), indent=2))
    elif op in {"start", "stop", "restart"} and len(sys.argv) > 2:
        name = sys.argv[2]
        if name not in services:
            raise SystemExit(f"unknown service: {name}")
        service = services[name]
        if op == "start":
            print(start(service))
        elif op == "stop":
            stop(service)
        else:
            stop(service)
            print(start(service))
    elif op == "supervise":
        supervise()
    else:
        raise SystemExit("usage: supervisor.py status|start NAME|stop NAME|restart NAME|supervise")
