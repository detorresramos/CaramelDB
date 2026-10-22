"""The steps of a CSF build, for running one across processes or machines.

`carameldb.Caramel` runs a whole build in one process, and
`carameldb.distributed.build` runs one on an executor. Use these steps directly
to run a build on any other system. Three nouns:

    chunk      a slice of the input rows (keys + values) that one worker loads
    partition  a group of buckets, solved as one unit of work
    piece      the rows of one chunk that belong to one partition

and four steps:

    planner = Planner(config)                        # 1. plan
    while (task := planner.next_task()) is not None:
        planner.add_results([task(*load(c)) for c in chunks])   # run anywhere
    plan = planner.plan()

    partitions = plan.partitions(64)                 # 2. split each chunk
    pieces = [split(plan, *load(c), partitions) for c in chunks]

    solved = [solve(plan, [p[j] for p in pieces])    # 3. solve each partition
              for j in range(len(partitions))]

    csf = combine(plan, solved)                      # 4. combine

Planning takes one pass over the chunks, or three with a prefilter (count
values, collect the keys the filter must hold, recount the keys it lets
through); the planner hands out whatever each pass needs. Tasks, results,
plans, pieces and solved partitions are all small picklable values.

Given chunks in the same order, the result is byte-identical to `Caramel` on
the concatenated chunks, for any number of workers or partitions.
"""

from dataclasses import dataclass, field
from typing import List, Optional

import numpy as np

from . import _caramel as _c

KINDS = ("csf", "multiset", "ragged", "packed")


@dataclass(frozen=True)
class BuildConfig:
    """What to build. Takes the same options as `Caramel`."""

    kind: str  # csf | multiset | ragged | packed
    dtype: str  # uint32 | uint64
    prefilter: object = None
    shared_codebook: bool = False
    shared_filter: bool = False
    verbose: bool = False
    # The serialized C++ spec: a config unpickled on a worker, where the
    # prefilter object does not travel, still describes the same build.
    _spec: bytes = field(default=None, repr=False, compare=False)
    _lengths_spec: bytes = field(default=None, repr=False, compare=False)

    def __post_init__(self):
        if self.kind not in KINDS:
            raise ValueError(f"kind must be one of {KINDS}, got {self.kind!r}")
        if self.dtype not in ("uint32", "uint64"):
            raise ValueError(f"dtype must be uint32 or uint64, got {self.dtype!r}")
        if self.kind in ("ragged", "packed") and self.dtype != "uint32":
            raise ValueError(f"{self.kind} values must be uint32")
        if self.shared_filter and self.kind != "multiset":
            raise ValueError("shared_filter is only supported for multiset values")
        if self.shared_codebook and self.kind not in ("multiset", "ragged"):
            raise ValueError("shared_codebook is only supported for multiset and ragged values")
        if self.kind == "packed" and self.prefilter is not None:
            raise ValueError("prefilters are not supported with strategy='packed'")
        if self._spec is None:
            object.__setattr__(self, "_spec", _c._step_spec(
                self.prefilter, self.shared_filter, self.shared_codebook,
                self.kind == "packed", self.verbose))
            object.__setattr__(self, "_lengths_spec", _c._step_spec(
                self.prefilter, False, False, False, self.verbose))

    @staticmethod
    def for_values(values, prefilter=None, shared_codebook=False, shared_filter=False,
                   strategy=None, verbose=False):
        """The config `Caramel` would use for `values`."""
        kind, dtype = describe(values, strategy)
        return BuildConfig(kind, dtype, prefilter, shared_codebook, shared_filter, verbose)

    def __reduce__(self):
        return (BuildConfig, (self.kind, self.dtype, None, self.shared_codebook,
                              self.shared_filter, self.verbose, self._spec,
                              self._lengths_spec))

    def _lengths(self):
        # A ragged CSF stores its row lengths in a csf of their own.
        return BuildConfig("csf", "uint32", verbose=self.verbose,
                           _spec=self._lengths_spec, _lengths_spec=self._lengths_spec)

    def _parts(self):
        """The builds this config needs: its values, plus lengths if ragged."""
        return [self, self._lengths()] if self.kind == "ragged" else [self]

    @property
    def _csr(self):
        return self.kind in ("ragged", "packed")


def describe(values, strategy=None):
    """(kind, dtype) of some values, by the rules `Caramel` uses."""
    if strategy == "packed":
        return "packed", "uint32"
    if strategy is not None:
        raise ValueError(f"Unknown strategy: {strategy!r}. Use None or 'packed'.")
    if isinstance(values, tuple):
        return "ragged", "uint32"
    from . import _is_ragged

    if _is_ragged(values):
        return "ragged", "uint32"
    arr = np.asarray(values)
    if arr.ndim not in (1, 2):
        raise ValueError("Values must be 1D (csf) or 2D (multiset).")
    if not np.issubdtype(arr.dtype, np.integer):
        raise ValueError("Builds from chunks support integer values only.")
    if arr.size and np.issubdtype(arr.dtype, np.signedinteger) and arr.min() < 0:
        raise ValueError("Negative integer values are not supported.")
    wide = arr.dtype == np.uint64 or (arr.size and arr.max() > np.iinfo(np.uint32).max)
    return ("csf" if arr.ndim == 1 else "multiset"), ("uint64" if wide else "uint32")


def _row_lengths(values):
    if isinstance(values, tuple):
        return np.diff(np.asarray(values[1], dtype=np.uint64)).astype(np.uint32)
    return np.fromiter((len(r) for r in values), dtype=np.uint32, count=len(values))


# ── 1. Plan ─────────────────────────────────────────────────────────────────


@dataclass(frozen=True)
class ChunkTask:
    """One planning pass over a chunk. Picklable: ship it to the chunk's worker
    and call it with the chunk's keys and values."""

    jobs: tuple  # per build part: (step, config, data) or None

    def __call__(self, keys, values):
        out = []
        for part, job in enumerate(self.jobs):
            if job is None:
                out.append(None)
                continue
            step, config, data = job
            vals = _row_lengths(values) if part == 1 else values
            if step == "count":
                out.append(_c._step_count(config.dtype, config._spec, keys, vals,
                                          config._csr, data))
            else:  # "exceptions"
                out.append(_c._step_find_exceptions(config.dtype, data, keys, vals,
                                                    config._csr))
        return tuple(out)


class _PartPlanner:
    """Plans one build part: count, then with a prefilter collect the keys the
    filters must hold and recount the keys they pass."""

    def __init__(self, config):
        self.config = config
        self.step, self.data = "count", b""  # next pass and its input
        self.counts = self.choice = self.filters = None
        self.plan = None

    def job(self):
        return None if self.plan is not None else (self.step, self.config, self.data)

    def add(self, results):
        c = self.config
        if self.step == "count":
            self.counts = _c._step_merge_counts(c.dtype, list(results))
            if self.filters is None:
                choice, num = _c._step_choose_filters(c.dtype, c._spec, self.counts)
                if num:
                    self.choice, self.step, self.data = choice, "exceptions", choice
                    return
        else:
            exceptions = _c._step_merge_exceptions(list(results))
            filters, num = _c._step_build_filters(c.dtype, self.choice, exceptions)
            if num:
                self.filters, self.step, self.data = filters, "count", filters
                return
        self.plan = _c._step_make_plan(c.dtype, c._spec, self.counts, self.filters or b"")


class Planner:
    """Turns passes over the chunks into a Plan. Run each task it hands out on
    every chunk and add the results in chunk order, until it has no more."""

    def __init__(self, config: BuildConfig):
        self.config = config
        self._parts = [_PartPlanner(c) for c in config._parts()]

    def next_task(self) -> Optional[ChunkTask]:
        jobs = tuple(p.job() for p in self._parts)
        return None if all(j is None for j in jobs) else ChunkTask(jobs)

    def add_results(self, results: list):
        """results[i] is the task's result on chunk i."""
        for part, planner in enumerate(self._parts):
            if planner.plan is None:
                planner.add([r[part] for r in results])

    def plan(self) -> "Plan":
        if any(p.plan is None for p in self._parts):
            raise RuntimeError("Planning needs more passes; call next_task().")
        lengths = self._parts[1].plan if len(self._parts) > 1 else None
        return Plan(self.config.kind, self.config.dtype, self._parts[0].plan, lengths)


@dataclass(frozen=True)
class Partition:
    """Buckets [first, end) of one column group (of the row lengths, for the
    lengths part of a ragged CSF)."""

    group: int
    first: int
    end: int
    lengths: bool = False


@dataclass(frozen=True)
class Plan:
    """Codebooks, filters and bucket counts: what every worker needs."""

    kind: str
    dtype: str
    data: bytes
    lengths_data: Optional[bytes] = None  # ragged only

    def partitions(self, count: int) -> List[Partition]:
        """Splits the work into about `count` partitions per column group."""
        out = [Partition(*p) for p in _c._step_partitions(self.dtype, self.data, count)]
        if self.lengths_data is not None:
            out += [Partition(*p, lengths=True)
                    for p in _c._step_partitions("uint32", self.lengths_data, count)]
        return out


# ── 2. Split, 3. Solve, 4. Combine ─────────────────────────────────────────


@dataclass(frozen=True)
class Piece:
    """The rows of one chunk that belong to one partition."""

    partition: Partition
    data: bytes


@dataclass(frozen=True)
class SolvedPartition:
    partition: Partition
    data: bytes


def split(plan: Plan, keys, values, partitions: List[Partition]) -> List[Piece]:
    """One piece per partition, in the order of `partitions`."""
    out = {}
    for lengths in (False, True):
        mine = [p for p in partitions if p.lengths == lengths]
        if not mine:
            continue
        if lengths:
            data = _c._step_split("uint32", plan.lengths_data, keys, _row_lengths(values),
                                  False, [(p.group, p.first, p.end) for p in mine])
        else:
            data = _c._step_split(plan.dtype, plan.data, keys, values,
                                  plan.kind in ("ragged", "packed"),
                                  [(p.group, p.first, p.end) for p in mine])
        out.update(zip(mine, data))
    return [Piece(p, out[p]) for p in partitions]


def solve(plan: Plan, pieces: List[Piece]) -> SolvedPartition:
    """Solves one partition from its pieces, one per chunk, in chunk order."""
    partition = pieces[0].partition
    if partition.lengths:
        data = _c._step_solve("uint32", plan.lengths_data, [p.data for p in pieces])
    else:
        data = _c._step_solve(plan.dtype, plan.data, [p.data for p in pieces])
    return SolvedPartition(partition, data)


def combine(plan: Plan, solved: List[SolvedPartition]):
    """The finished CSF, the same object `Caramel` returns."""
    values = [s.data for s in solved if not s.partition.lengths]
    if plan.kind != "ragged":
        return _c._step_combine(plan.kind, plan.dtype, plan.data, values)
    lengths = _c._step_combine("csf", "uint32", plan.lengths_data,
                               [s.data for s in solved if s.partition.lengths])
    return _c._step_combine("ragged", "uint32", plan.data, values, lengths)


def build(config: BuildConfig, keys, values, num_partitions=1):
    """Every step in this process, on one chunk: the same result as `Caramel`."""
    planner = Planner(config)
    while (task := planner.next_task()) is not None:
        planner.add_results([task(keys, values)])
    plan = planner.plan()
    pieces = split(plan, keys, values, plan.partitions(num_partitions))
    return combine(plan, [solve(plan, [p]) for p in pieces])
