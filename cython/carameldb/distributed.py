"""Build a CSF across many processes or machines.

    from carameldb import AutoFilterConfig
    from carameldb.distributed import build

    csf = build(chunks, prefilter=AutoFilterConfig())

`chunks` is a list of picklable zero-argument callables. Each one loads one
slice of the input on a worker and returns (keys, values), in the formats
`Caramel` accepts. The other arguments mean what they mean for `Caramel`, and
the result is byte-identical to `Caramel` on the concatenated chunks.

Where the work runs is up to `executor`, anything with the
`concurrent.futures.Executor` interface:

    default                      a process pool on this machine
    RayExecutor()                a Ray cluster (pip install ray)
    dask_client.get_executor()   a Dask cluster

To run the steps on some other system, use `carameldb.steps`; `_build` below
is the whole recipe.

Every step result passes through this process. That suits inputs whose
split pieces fit in the driver's memory; beyond that, have the steps read and
write shared storage and pass paths instead.
"""

import os
from concurrent.futures import Executor, Future, ProcessPoolExecutor
from multiprocessing import get_context

from . import AutoFilterConfig, steps


def build(chunks, prefilter=AutoFilterConfig(), shared_codebook=False,
          shared_filter=False, strategy=None, executor=None, num_partitions=None,
          verbose=False):
    """Builds a CSF from `chunks` on `executor`.

    num_partitions: how many solve tasks each column group is split into.
    Defaults to one per chunk.
    """
    chunks = list(chunks)
    owned = executor is None
    if owned:
        executor = local_executor(len(chunks))
    try:
        config = _config(executor, chunks, prefilter, shared_codebook, shared_filter,
                         strategy, verbose)
        return _build(config, chunks, executor, num_partitions or len(chunks))
    finally:
        if owned:
            executor.shutdown()


def _config(executor, chunks, prefilter, shared_codebook, shared_filter, strategy, verbose):
    # The kind and dtype `Caramel` would infer for the concatenated chunks:
    # one ragged chunk makes the whole input ragged, one wide chunk makes it
    # uint64.
    described = _each(executor, _describe, chunks, strategy)
    kinds = {kind for kind, _ in described}
    if kinds == {"ragged", "multiset"}:
        kinds = {"ragged"}
    if len(kinds) != 1:
        raise ValueError(f"Chunks disagree on the kind of values: {sorted(kinds)}")
    kind = kinds.pop()
    dtype = "uint64" if any(d == "uint64" for _, d in described) else "uint32"
    if kind == "packed":
        prefilter = None  # as in Caramel: packed CSFs take no prefilter
    return steps.BuildConfig(kind, dtype, prefilter, shared_codebook, shared_filter, verbose)


def _build(config, chunks, executor, num_partitions):
    planner = steps.Planner(config)
    while (task := planner.next_task()) is not None:
        planner.add_results(_each(executor, _run, chunks, task))
    plan = planner.plan()

    partitions = plan.partitions(num_partitions)
    pieces = _each(executor, _split, chunks, plan, partitions)  # [chunk][partition]
    solved = [f.result() for f in [
        executor.submit(steps.solve, plan, [chunk_pieces[j] for chunk_pieces in pieces])
        for j in range(len(partitions))]]
    return steps.combine(plan, solved)


def _each(executor, task, chunks, *args):
    """Runs task(chunk, *args) for every chunk; results in chunk order."""
    futures = [executor.submit(task, chunk, *args) for chunk in chunks]
    return [f.result() for f in futures]


def _describe(chunk, strategy):
    _, values = chunk()
    return steps.describe(values, strategy)


def _run(chunk, task):
    return task(*chunk())


def _split(chunk, plan, partitions):
    keys, values = chunk()
    return steps.split(plan, keys, values, partitions)


def _limit_threads(threads):
    os.environ["OMP_NUM_THREADS"] = str(threads)


def local_executor(num_tasks, max_workers=None):
    """A process pool on this machine. Workers split the cores between them so
    their OpenMP threads do not oversubscribe it."""
    cores = os.cpu_count() or 1
    workers = max(1, min(max_workers or cores, num_tasks, cores))
    return ProcessPoolExecutor(
        max_workers=workers, mp_context=get_context("spawn"),
        initializer=_limit_threads, initargs=(max(1, cores // workers),))


class RayExecutor(Executor):
    """The Executor interface over Ray tasks. `options` go to every task's
    `.options()`, e.g. RayExecutor(num_cpus=8)."""

    def __init__(self, **options):
        import ray

        if not ray.is_initialized():
            ray.init()
        self._ray = ray
        self._options = options
        self._remotes = {}

    def submit(self, fn, /, *args, **kwargs) -> Future:
        remote = self._remotes.get(fn)
        if remote is None:
            remote = self._remotes[fn] = self._ray.remote(fn)
        return remote.options(**self._options).remote(*args, **kwargs).future()
