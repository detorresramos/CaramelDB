import os

import carameldb
import numpy as np
import pytest

N = 2000
KEYS = [f"key{i}" for i in range(N)]
_rng = np.random.default_rng(0)

BUILDS = {
    "csf_uint32": lambda: (_rng.integers(0, 50, N).astype(np.uint32), {}),
    "csf_uint64": lambda: (_rng.integers(2**40, 2**40 + 50, N).astype(np.uint64), {}),
    "csf_string": lambda: (np.array([f"v{i % 30}" for i in range(N)]), {}),
    "multiset": lambda: (_rng.integers(0, 20, (N, 6)).astype(np.uint32), {}),
    "multiset_shared": lambda: (
        _rng.integers(0, 20, (N, 6)).astype(np.uint32),
        {"shared_codebook": True},
    ),
    "ragged": lambda: ([list(range(i % 5 + 1)) for i in range(N)], {}),
    "packed": lambda: ([list(range(i % 5 + 1)) for i in range(N)], {"strategy": "packed"}),
}


@pytest.mark.parametrize("kind", BUILDS)
def test_load_restores_every_type(kind, tmp_path):
    values, kwargs = BUILDS[kind]()
    csf = carameldb.Caramel(KEYS, values, verbose=False, **kwargs)
    path = str(tmp_path / "csf")
    csf.save(path)
    loaded = carameldb.load(path)
    assert type(loaded) is type(csf)
    assert [loaded.query(k) for k in KEYS] == [csf.query(k) for k in KEYS]


def _header_file(path):
    return os.path.join(path, "metadata.bin") if os.path.isdir(path) else path


@pytest.mark.parametrize("kind", ["csf_uint32", "multiset", "ragged", "packed"])
def test_load_rejects_another_format_version(kind, tmp_path):
    # A file written before a format change must fail to load, not decode
    # garbage lengths into an unbounded allocation.
    values, kwargs = BUILDS[kind]()
    path = str(tmp_path / "csf")
    carameldb.Caramel(KEYS, values, verbose=False, **kwargs).save(path)
    with open(_header_file(path), "r+b") as f:
        f.seek(4)
        f.write((1).to_bytes(4, "little"))
    with pytest.raises(ValueError, match="does not contain a deserializable CSF"):
        carameldb.load(path)


@pytest.mark.parametrize("kind", ["csf_uint32", "ragged", "packed"])
def test_load_rejects_truncated_file(kind, tmp_path):
    values, kwargs = BUILDS[kind]()
    path = str(tmp_path / "csf")
    carameldb.Caramel(KEYS, values, verbose=False, **kwargs).save(path)
    size = os.path.getsize(path)
    with open(path, "r+b") as f:
        f.truncate(size // 2)
    with pytest.raises(ValueError, match="does not contain a deserializable CSF"):
        carameldb.load(path)
