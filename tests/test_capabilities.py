import importlib.util
from pathlib import Path


MODULE_PATH = Path(__file__).parents[1] / "core" / "capabilities.py"


def load_module():
    spec = importlib.util.spec_from_file_location("vibecode_capabilities", MODULE_PATH)
    assert spec and spec.loader
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def test_snapshot_has_stable_schema(monkeypatch):
    module = load_module()
    monkeypatch.setattr(module, "read_prop", lambda name: {"ro.build.version.sdk": "35", "ro.product.cpu.abi": "x86_64"}.get(name))
    monkeypatch.setattr(module, "root_available", lambda: False)
    monkeypatch.setattr(module, "thermal_zones", lambda: [])
    data = module.snapshot()
    for key in ("os", "kernel", "machine", "python", "termux", "android_api", "android_abi", "root_available", "cpu_count", "thermal_zones"):
        assert key in data
    assert data["android_api"] == "35"
    assert data["android_abi"] == "x86_64"
    assert data["root_available"] is False
