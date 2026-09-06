#!/usr/bin/env python3
from __future__ import annotations

import os
import signal
import subprocess
import time
from dataclasses import dataclass
from pathlib import Path

HOME = Path.home() / '.vibecode'
CONF = HOME / 'services.conf'
RUN = HOME / 'run'
LOG = HOME / 'logs' / 'services'

@dataclass
class Service:
    name: str
    command: str
    restart: bool = True
    delay: float = 1.0


def load() -> list[Service]:
    out: list[Service] = []
    if not CONF.exists(): return out
    for raw in CONF.read_text(encoding='utf-8').splitlines():
        line = raw.strip()
        if not line or line.startswith('#') or '=' not in line: continue
        name, command = line.split('=', 1)
        out.append(Service(name.strip(), command.strip()))
    return out


def path_for(name: str) -> Path:
    return RUN / f'{name}.pid'


def running(name: str) -> int | None:
    p = path_for(name)
    if not p.exists(): return None
    try:
        pid = int(p.read_text().strip())
        os.kill(pid, 0)
        return pid
    except (ValueError, ProcessLookupError, PermissionError):
        p.unlink(missing_ok=True)
        return None


def start(service: Service) -> int:
    existing = running(service.name)
    if existing: return existing
    RUN.mkdir(parents=True, exist_ok=True); (LOG / service.name).mkdir(parents=True, exist_ok=True)
    log = (LOG / service.name / 'stdout.log').open('ab')
    p = subprocess.Popen(service.command, shell=True, stdout=log, stderr=subprocess.STDOUT, start_new_session=True)
    path_for(service.name).write_text(str(p.pid))
    return p.pid


def stop(service: Service) -> None:
    pid = running(service.name)
    if pid is not None:
        try: os.kill(pid, signal.SIGTERM)
        except ProcessLookupError: pass
        path_for(service.name).unlink(missing_ok=True)


def snapshot() -> list[dict[str, object]]:
    return [{'name': s.name, 'command': s.command, 'pid': running(s.name)} for s in load()]

if __name__ == '__main__':
    import json, sys
    op = sys.argv[1] if len(sys.argv) > 1 else 'status'
    services = {s.name: s for s in load()}
    if op == 'status': print(json.dumps(snapshot(), indent=2))
    elif op == 'start' and len(sys.argv) > 2: print(start(services[sys.argv[2]]))
    elif op == 'stop' and len(sys.argv) > 2: stop(services[sys.argv[2]])
    else: raise SystemExit('usage: supervisor.py status|start NAME|stop NAME')
