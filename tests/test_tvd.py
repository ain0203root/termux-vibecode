from pathlib import Path
import importlib.util

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('tvd', ROOT / 'core' / 'tvd.py')
assert spec and spec.loader
mod = importlib.util.module_from_spec(spec)
spec.loader.exec_module(mod)


def test_status_has_machine_fields():
    data = mod.status()
    assert 'machine' in data
    assert 'cpu_count' in data
    assert data['cpu_count'] is None or data['cpu_count'] >= 1


def test_doctor_shape():
    data = mod.doctor()
    assert isinstance(data['checks'], dict)
    assert isinstance(data['missing'], list)
