from pathlib import Path


def test_supervisor_source_is_syntax_valid(tmp_path):
    source = Path(__file__).parents[1] / "core" / "supervisor.py"
    compile(source.read_text(encoding="utf-8"), str(source), "exec")


def test_service_config_is_parseable():
    config = Path(__file__).parents[1] / "services" / "services.conf"
    for line in config.read_text(encoding="utf-8").splitlines():
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        name, command = line.split("=", 1)
        assert name.strip()
        assert command.strip()
