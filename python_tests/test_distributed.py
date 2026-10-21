import hashlib
import os
import pickle

import numpy as np
import pytest

import carameldb
from carameldb import distributed, steps

N = 6000
CUTS = [0, 1700, 4100, N]


def dataset(kind, wide=False):
    rng = np.random.default_rng(0)
    keys = [f"key{i}" for i in range(N)]
    if kind in ("csf", "multiset"):
        shape = (N,) if kind == "csf" else (N, 5)
        values = rng.integers(1, 40, size=shape).astype(np.uint64 if wide else np.uint32)
        values[rng.random(shape) < 0.8] = 0
        if wide:
            values[values > 0] += np.uint64(1 << 40)
        return keys, values
    lengths = rng.integers(1, 7, size=N)
    return keys, [rng.integers(0, 30, size=k).astype(np.uint32).tolist() for k in lengths]


class Chunk:
    """Rows [lo, hi) of a dataset; picklable, loaded on the worker."""

    def __init__(self, kind, lo, hi, wide=False):
        self.kind, self.lo, self.hi, self.wide = kind, lo, hi, wide

    def __call__(self):
        keys, values = dataset(self.kind, self.wide)
        return keys[self.lo:self.hi], values[self.lo:self.hi]


def chunks(kind, wide=False):
    return [Chunk(kind, a, b, wide) for a, b in zip(CUTS, CUTS[1:])]


def digest(csf, tmp_path):
    os.makedirs(tmp_path, exist_ok=True)
    path = os.path.join(tmp_path, "csf")
    csf.save(path)
    h = hashlib.sha256()
    names = sorted(os.listdir(path)) if os.path.isdir(path) else [""]
    for name in names:
        h.update(open(os.path.join(path, name) if name else path, "rb").read())
    return h.hexdigest()


@pytest.fixture(scope="module")
def executor():
    with distributed.local_executor(2) as ex:
        yield ex


FILTERS = {
    None: lambda: None,
    "auto": carameldb.AutoFilterConfig,
    "bloom": lambda: carameldb.BloomFilterConfig(8, 3),
    "xor": lambda: carameldb.XORFilterConfig(8),
    "binary_fuse": lambda: carameldb.BinaryFuseFilterConfig(8),
}

CASES = [
    ("csf", {"prefilter": None}),
    ("csf", {"prefilter": "auto"}),
    ("csf", {"prefilter": "bloom"}),
    ("csf", {"prefilter": "xor"}),
    ("csf", {"prefilter": "binary_fuse"}),
    ("multiset", {"prefilter": None}),
    ("multiset", {"prefilter": "auto"}),
    ("multiset", {"prefilter": "auto", "shared_filter": True}),
    ("multiset", {"prefilter": "xor", "shared_filter": True, "shared_codebook": True}),
    ("ragged", {"prefilter": "auto"}),
    ("ragged", {"prefilter": None, "shared_codebook": True}),
    ("packed", {"strategy": "packed"}),
]


@pytest.mark.parametrize("kind,options", CASES, ids=str)
def test_distributed_build_equals_caramel(kind, options, executor, tmp_path):
    options = dict(options)
    if "prefilter" in options:
        options["prefilter"] = FILTERS[options["prefilter"]]()
    keys, values = dataset(kind)

    expected = carameldb.Caramel(keys, values, verbose=False, **options)
    built = distributed.build(chunks(kind), executor=executor, num_partitions=4, **options)

    assert type(built) is type(expected)
    assert digest(built, tmp_path / "a") == digest(expected, tmp_path / "b")


def test_one_wide_chunk_makes_the_build_uint64(executor):
    parts = chunks("multiset", wide=True)
    parts[0] = Chunk("multiset", 0, CUTS[1])  # narrow
    built = distributed.build(parts, executor=executor)
    assert isinstance(built, carameldb.MultisetCSFUint64)
    for part in parts:
        keys, values = part()
        for i in range(0, len(keys), 97):
            assert built.query(keys[i]) == list(values[i])


def test_steps_in_one_process_equal_caramel(tmp_path):
    keys, values = dataset("multiset")
    config = steps.BuildConfig.for_values(values, prefilter=carameldb.AutoFilterConfig())
    assert (config.kind, config.dtype) == ("multiset", "uint32")
    built = steps.build(config, keys, values, num_partitions=5)
    expected = carameldb.Caramel(keys, values, prefilter=carameldb.AutoFilterConfig(),
                                 verbose=False)
    assert digest(built, tmp_path / "a") == digest(expected, tmp_path / "b")


def run_steps(config, loaded, num_partitions, reverse_solved=False):
    """The four steps by hand, with every value pickled between steps."""
    rt = lambda x: pickle.loads(pickle.dumps(x))
    planner = steps.Planner(rt(config))
    passes = 0
    while (task := planner.next_task()) is not None:
        planner.add_results([rt(rt(task)(k, v)) for k, v in loaded])
        passes += 1
    plan = rt(planner.plan())
    partitions = plan.partitions(num_partitions)
    pieces = [rt(steps.split(plan, k, v, partitions)) for k, v in loaded]
    solved = [rt(steps.solve(plan, [p[j] for p in pieces])) for j in range(len(partitions))]
    return steps.combine(plan, solved[::-1] if reverse_solved else solved), passes


@pytest.mark.parametrize("kind,prefilter,passes", [
    ("csf", None, 1), ("csf", "auto", 3), ("multiset", "xor", 3), ("ragged", "auto", 2),
    ("packed", None, 1)])
def test_steps_by_hand_equal_caramel(kind, prefilter, passes, tmp_path):
    keys, values = dataset(kind)
    options = {"strategy": "packed"} if kind == "packed" else {"prefilter": FILTERS[prefilter]()}
    config = steps.BuildConfig.for_values(values, **options)
    built, ran = run_steps(config, [c() for c in chunks(kind)], 3)
    assert ran == passes
    expected = carameldb.Caramel(keys, values, verbose=False, **options)
    assert digest(built, tmp_path / "a") == digest(expected, tmp_path / "b")


def test_chunk_order_changes_bytes_not_answers():
    keys, values = dataset("csf")
    config = steps.BuildConfig("csf", "uint32", prefilter=carameldb.AutoFilterConfig())
    csf, _ = run_steps(config, [c() for c in chunks("csf")][::-1], 3, reverse_solved=True)
    for i in range(0, N, 7):
        assert csf.query(keys[i]) == values[i]


def test_config_validation():
    with pytest.raises(ValueError):
        steps.BuildConfig("tree", "uint32")
    with pytest.raises(ValueError):
        steps.BuildConfig("csf", "uint32", shared_filter=True)
    with pytest.raises(ValueError):
        steps.BuildConfig("packed", "uint32", prefilter=carameldb.AutoFilterConfig())
    with pytest.raises(ValueError):
        steps.BuildConfig("ragged", "uint64")
    with pytest.raises(ValueError):
        steps.describe(np.array([1.5, 2.5]))


def test_ray_executor(tmp_path, monkeypatch):
    # Under `uv run`, Ray starts workers by re-running the uv command line;
    # they only need this interpreter.
    monkeypatch.setenv("RAY_ENABLE_UV_RUN_RUNTIME_ENV", "0")
    ray = pytest.importorskip("ray")
    # Ray workers import Chunk from this file.
    here = os.path.dirname(os.path.abspath(__file__))
    ray.init(num_cpus=2, include_dashboard=False,
             runtime_env={"env_vars": {"PYTHONPATH": here}})
    try:
        keys, values = dataset("multiset")
        built = distributed.build(chunks("multiset"), executor=distributed.RayExecutor(),
                                  num_partitions=3)
        expected = carameldb.Caramel(keys, values, verbose=False)
        assert digest(built, tmp_path / "a") == digest(expected, tmp_path / "b")
    finally:
        ray.shutdown()
