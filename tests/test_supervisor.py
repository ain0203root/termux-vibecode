import importlib.util
import os
import time
from pathlib import Path


ROOT = Path(__file__).parents[1]
SOURCE = ROOT / "core" / "supervisor.py"


def load_module():
    spec = importlib.util.spec_from_file_location("vibecode_supervisor", SOURCE)
    assert spec and spec.loader
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def test_supervisor_source_is_syntax_valid():
    compile(SOURCE.read_text(encoding="utf-8"), str(SOURCE), "exec")


def test_service_config_is_parseable():
    config = ROOT / "services" / "services.conf"
    for line in config.read_text(encoding="utf-8").splitlines():
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        name, command = line.split("=", 1)
        assert name.strip()
        assert command.strip()


def test_start_snapshot_and_stop_are_lifecycle_safe(tmp_path):
    supervisor = load_module()
    supervisor.HOME = tmp_path / ".vibecode"
    supervisor.CONF = supervisor.HOME / "services.conf"
    supervisor.RUN = supervisor.HOME / "run"
    supervisor.LOG = supervisor.HOME / "logs" / "services"
    supervisor.CONF.parent.mkdir(parents=True)
    supervisor.CONF.write_text("probe=python3 -c 'import time; time.sleep(30)'\n", encoding="utf-8")

    service = supervisor.load()[0]
    pid = supervisor.start(service)
    try:
        assert pid > 0
        assert supervisor.running(service.name) == pid

        record = supervisor.path_for(service.name).read_text(encoding="utf-8")
        assert '"pid":' in record
        assert '"starttime":' in record
        assert '"command":' in record

        snapshot = supervisor.snapshot()
        assert snapshot == [{"name": "probe", "command": service.command, "restart": True, "pid": pid}]
    finally:
        supervisor.stop(service)

    assert supervisor.running(service.name) is None
    assert not supervisor.path_for(service.name).exists()


def test_pid_record_does_not_accept_reused_process_identity(tmp_path):
    supervisor = load_module()
    supervisor.HOME = tmp_path / ".vibecode"
    supervisor.RUN = supervisor.HOME / "run"
    supervisor.RUN.mkdir(parents=True)
    path = supervisor.path_for("probe")
    path.write_text(
        '{"pid": %d, "starttime": 1, "command": "probe"}\n' % os.getpid(),
        encoding="utf-8",
    )
    assert supervisor.running("probe") is None
    assert not path.exists()
