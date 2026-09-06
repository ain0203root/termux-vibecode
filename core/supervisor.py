#!/usr/bin/env python3
from __future__ import annotations

import json
import os
import signal
import subprocess
import time
from dataclasses import dataclass
from pathlib import Path

HOME = Path.home() / ".vibecode"
CONF = HOME / "services.conf"
RUN = HOME / "run"
LOG = HOME / "logs" / "services"

@dataclass(frozen=True)
class Service:
    name: str
    command: str
    restart: bool = True
    delay: float = 1.0


def load() -> list[Service]:
    out: list[Service] = []
    if not CONF.exists():
        return out
    for raw in CONF.read_text(encoding="utf-8").splitlines():
        line = raw.strip()
        if not line or line.startswith("#") or "=" not in line:
            continue
        name, command = line.split("=", 1)
        if name.strip() and command.strip():
            out.append(Service(name.strip(), command.strip()))
    return out


def path_for(name: str) -> Path:
    return RUN / f"{name}.pid"


def running(name: str) -> int | None:
    path = path_for(name)
    if not path.exists():
        return None
    try:
        pid = int(path.read_text(encoding="utf-8").strip())
        os.kill(pid, 0)
        return pid
    except (ValueError, ProcessLookupError, PermissionError):
        path.unlink(missing_ok=True)
        return None


def start(service: Service) -> int:
    existing = running(service.name)
    if existing:
        return existing
    RUN.mkdir(parents=True, exist_ok=True)
    log_dir = LOG / service.name
    log_dir.mkdir(parents=True, exist_ok=True)
    log = (log_dir / "stdout.log").open("ab")
    proc = subprocess.Popen(
        service.command,
        shell=True,
        stdout=log,
        stderr=subprocess.STDOUT,
        start_new_session=True,
        close_fds=True,
    )
    path_for(service.name).write_text(str(proc.pid), encoding="utf-8")
    (log_dir / "started_at").write_text(str(time.time()), encoding="utf-8")
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
    if running(service.name) is not None:
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
    for service in services:
        start(service)
    while True:
        for service in services:
            if running(service.name) is None and service.restart:
                start(service)
        time.sleep(1.0)


if __name__ == "__main__":
    import sys
    op = sys.argv[1] if len(sys.argv) > 1 else "status"
    services = {s.name: s for s in load()}
    if op == "status":
        print(json.dumps(snapshot(), indent=2))
    elif op == "start" and len(sys.argv) > 2:
        print(start(services[sys.argv[2]]))
    elif op == "stop" and len(sys.argv) > 2:
        stop(services[sys.argv[2]])
    elif op == "restart" and len(sys.argv) > 2:
        stop(services[sys.argv[2]]); print(start(services[sys.argv[2]]))
    elif op == "supervise":
        supervise()
    else:
        raise SystemExit("usage: supervisor.py status|start NAME|stop NAME|restart NAME|supervise")
