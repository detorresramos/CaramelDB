# Multiset query latency, July 2026

Work on top of `exp/interleaved-layout` (`c7072d6`), on branch `exp/query-latency`.
Machine: Apple M4 Pro, 14 cores, 24 GB. Dataset: MovieLens, 1M rows, uint32
values, `global_sort` permutation with 10 refinement iterations, `prefilter=auto`
(which resolves to no filters here, so all columns land in one group).

## Result

C++-level latency, both paths timed in one binary against the same index
(`bench/QueryBench.cc`; "before" is the per-column loop with the bit-at-a-time
decode, i.e. the code as it was on `exp/interleaved-layout`):

| | before | after | batch API | speedup |
|---|---|---|---|---|
| 1M × 100 | 4117 ns | 1752 ns | 1543 ns | 2.35× / 2.67× |
| 1M × 20 | 715 ns | 322 ns | 227 ns | 2.22× / 3.15× |

Other axes, measured end to end through `scripts/benchmark_multiset.py`:

| 1M × 100 | before | after |
|---|---|---|
| serialized size | 111,980,613 B | 109,350,505 B (−2.3%) |
| build time | 40.85 s | 40.42 s |
| peak RSS | 3446 MB | 3612 MB |
| python-level query | 5434 ns | 3438 ns |

| 1M × 20 | before | after |
|---|---|---|
| serialized size | 28,300,453 B | 27,642,865 B (−2.3%) |
| build time | 11.36 s | 10.97 s |
| peak RSS | 1605 MB | 1571–1677 MB |

Peak RSS varies ±7% run to run on this machine (two runs of the identical
post-change m=20 configuration gave 1571 MB and 1677 MB), so the RSS column is
unchanged rather than moved. Build time is unchanged. Size is down because the
arena's per-(bucket, column) word offsets are no longer serialized and the solve
seeds are stored as one byte.

The python-level numbers above come from `scripts/benchmark_multiset.py`, which
times 250 freshly-sampled keys per trial in the process that just did the build;
they are inflated relative to a steady-state measurement (see below) but are
measured identically before and after, so the ratio holds.

## Binding cost

Measured in one process against the m=100 index, comparing `query()` against a
probe that runs the same C++ query but returns a single value instead of a list:

| | `query()` | C++ only | binding | per column |
|---|---|---|---|---|
| m=100 | 2268 ns | 1609 ns | 660 ns (29%) | 6.6 ns |
| m=20 | 487 ns | 311 ns | 176 ns (36%) | 8.8 ns |

The Cython call itself is ~13 ns; effectively all of the binding cost is
building the result list — one `PyLong` per column, and the values here exceed
CPython's small-int cache. Against ~16 ns per column of actual lookup work, that
is ~40% overhead, not the dominant term.

Returning a numpy array instead of a list measured 1764 ns at m=100 (saving
~500 ns, 22%) and 482 ns at m=20 (a wash — `np.empty` has a fixed ~150 ns cost
that only pays off for large m). Not changed here; it would alter the public
return type.

## Where the time goes

Measured by cutting the query short at each stage (1M × 100, before the changes
below):

| stage | ns/query | per column |
|---|---|---|
| variable positions only (no prefetch, no reads) | 395 | 4.0 ns |
| + arena reads | 1635 | |
| + canonical decode | 3840 | |

So the fan-out was roughly 32% memory stall, 57% decode, 10% position
arithmetic. The two changes below target the second and first of those.

## What landed

1. **Prefetch pipeline in `Group::queryAll`.** The columns of a bucket are
   independent, but each column's work is hash → read → decode, so issuing them
   one at a time leaves every read exposed. The loop now walks the group in
   chunks of 12: it computes the three variable positions for chunk *k+1* and
   prefetches them before decoding chunk *k*. Chunk size 12 measured best
   (8–48 all within ~10%; below 8 and above 32 it degrades).

2. **Branchless canonical decode.** `canonicalDecodeFromNumber` walked the code
   one bit at a time with a data-dependent exit, ~9 mispredicted iterations per
   column. For a canonical code the length is the smallest *l* whose left-aligned
   limit exceeds the value, and those limits are non-decreasing, so the length is
   a count: `for l: length += (value >= limit[l])`. Two tables of
   `max_codelength+1` entries per codebook, built at load, not serialized.
   `CodecTest.BranchlessDecodeMatchesReference` checks it against the reference
   decoder over every codeword with random trailing bits.

3. **`MultisetCsf::queryBatch`.** Interleaves 8 keys through a group at once.
   Worth ~10% at m=100 and ~30% at m=20 over the single-key path.

4. **Arena metadata compaction.** `per_col_word_off` is a running sum of the
   per-column range sizes, so it is rebuilt from `per_col_bits` on load instead
   of being serialized (8 B/cell); solve seeds are a retry counter bounded by the
   solver's 128-attempt limit, so they are stored as `uint8` (3 B/cell). −2.3%
   on disk at any bucket count.

## What did not work

- **Row fusion** — one linear system per bucket whose value is the concatenation
  of the row's *m* codewords, so a query does 3 wide window reads instead of 3*m*
  scattered ones. This was the plan, and `bench/FusedProbe.cc` measured it at the
  real geometry before implementing it. It loses:

  | buckets | current layout | fused |
  |---|---|---|
  | 2,391 | 1544 ns | 1501 ns |
  | 10,001 | 1241 ns | 1479 ns |

  The reason is that concatenated variable-length codewords must be decoded
  **serially** — column *c*'s bit offset is the sum of the preceding codeword
  lengths — turning *m* independent ~40-cycle decode chains into one chain of
  length *m*. That costs more than the ~290 cache lines it saves. Padding each
  column's field to a fixed width would restore parallel decoding but costs
  `max_codelength / avg_codelength` ≈ 2.2× in space.

- **Cheaper equation hash.** Replacing SpookyHash's 12-round `ShortMix` with a
  wyhash-style multiply-fold in `signatureToEquation` cut the position stage from
  395 ns to 340 ns — 1.4% of the query, and it would have changed the
  construction-side hash. Not worth the risk.

- **Slab prefetch.** Prefetching the bucket's whole contiguous slab up front,
  instead of the 3*m* computed addresses, was 10–25% *slower* everywhere: the
  computed prefetches already cover the lines that are actually read.

- **Packing the query cache to 8 bytes** (arena word offset instead of a pointer,
  seed folded into the spare bits of the variable count). Halves the 1600 B of
  `BucketQueryInfo` a query streams; no measurable latency change. Reverted.

## Tunable: bucket size

Bucket count is not currently a knob (`TARGET_EQUATIONS_PER_BUCKET = 3500`, with
`num_buckets` clamped to `num_keys/100 + 1`). Forcing it higher shrinks each
per-(bucket, column) range, so a column's 3 probes land in fewer cache lines.
Measured at 1M × 100 (C++ latency with the changes above):

| buckets | bits per (bucket, col) | latency | size | build | peak RSS |
|---|---|---|---|---|---|
| 2,391 (default) | 2689 | 1553 ns | 112.0 MB | 40.1 s | 4379 MB |
| 4,001 | 1680 | 1787 ns | 115.6 MB | 26.0 s | 3492 MB |
| 10,001 | 751 | 1366 ns | 129.1 MB | 17.4 s | 2381 MB |
| 20,001 | 378 | 1284 ns | 151.6 MB | 20.9 s | 1402 MB |
| 40,001 | 151 | 1259 ns | 196.7 MB | 25.4 s | 1470 MB |

Latency saturates near 10k buckets — past that each column's range is already
inside one cache line. Build time and peak RSS improve substantially (40 s → 17 s,
4.4 GB → 2.4 GB at 10k buckets), which is worth noting on its own. The size cost
at 10k buckets is +15%, of which ~12 MB is per-cell metadata; with the
compaction in (4) it would be roughly +6%, and bit-packing the ranges instead of
word-aligning them would take off another ~4 MB. That work was not done — the
table above is measured with the *uncompacted* metadata, so treat the size column
as an upper bound.

These runs used a temporary env override of the bucket-count formula, removed
before the final commit.

## Remaining headroom

After the changes, at 1M × 100 the query is roughly 400 ns of position
arithmetic, ~600 ns of arena reads, and ~390 ns of decode. The arena reads are
close to single-core memory bandwidth at the default bucket size (~300 probes ×
128 B lines ≈ 38 KB per query), so further gains have to come from touching fewer
cache lines — which means either smaller buckets (the table above) or a
retrieval structure with fewer probes per lookup, e.g. bumped ribbon (BuRR),
which lands ~1 cache line per query. That is a replacement for the solver, not a
tweak to it.
