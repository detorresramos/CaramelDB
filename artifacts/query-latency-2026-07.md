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

Measured in one process, comparing `query()` against a probe that runs the same
C++ query but returns a single value instead of a list:

| m | `query()` | C++ only | binding | binding/col | C++/col |
|---|---|---|---|---|---|
| 5 | 211 ns | 167 ns | 44 ns | 8.79 ns | 33.5 ns |
| 20 | 496 ns | 336 ns | 160 ns | 7.99 ns | 16.8 ns |
| 50 | 1149 ns | 776 ns | 373 ns | 7.46 ns | 15.5 ns |
| 100 | 2392 ns | 1671 ns | 721 ns | 7.21 ns | 16.7 ns |

Least squares over those four points:

- binding: **14 ns fixed + 7.10 ns per column**
- C++ query: **37 ns fixed + 16.01 ns per column**

The binding cost is almost entirely per-column, and that is not an artifact of
the bindings being slow — the Cython call itself is the 14 ns. The per-column
term is building the returned list: one `PyLong_FromUnsignedLong` plus a list
store for each of the m values, and the values here exceed CPython's small-int
cache, so each is a heap allocation. The *lookup* is internal; the *result
marshalling* is inherently O(m) work at the language boundary.

So the binding is ~30% of a Python-visible query at m=100 and ~36% at m=20 — it
grew in relative terms precisely because the C++ side got 2.35x faster. (The C++
fixed cost of 37 ns is the key hash, bucket selection and result vector
allocation, which is why m=5 shows a high 33 ns/column.)

That per-column term is not computation — the m values are already computed when
the C++ query returns. It is materialization. Cython generates this for
`return list(...query(...))` (`cython/_caramel.cpp:5778`):

    o = PyList_New(v_size_signed);
    for (i = 0; i < v_size_signed; i++) {
        item = __Pyx_PyLong_From_unsigned_int(v[i]);   // -> PyLong_FromLong
        __Pyx_PyList_SET_ITEM(o, i, item);
    }

Compiled C, but one `PyObject` allocation per column. `array.tolist()` does the
same work, and timing it on 100 uint32 values isolates the cost:

| | per element |
|---|---|
| 100 large ints -> list (allocates each PyLong) | 7.08 ns |
| 100 small ints -> list (< 257, CPython's cached objects) | 2.08 ns |
| `list(list_of_100)` (pointer copy, no object creation) | 2.08 ns |

7.08 ns against the 7.10 ns/column measured above. So ~5 ns per column is the
`PyLong` heap allocation (the values here exceed the small-int cache) and ~2 ns
is the loop, list store and refcounting. It cannot be made O(1) while `query()`
returns `list[int]`: m Python objects have to exist for the caller to index.

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

## Follow-up session (same day)

- **Branch-free window extract.** `getbits` had `if (b <= l)` on the in-word
  bit offset — uniform data, so it mispredicts. Replaced with a funnel window
  (`arr[w] << b | arr[w+1] >> 1 >> (63-b)`), with the `>> l` alignment hoisted
  out of the 3-way XOR. m=100: 1752 -> 1479 ns; m=20: 322 -> 250 ns. Free, no
  format change.

- **Pooled shared codebook: rejected for latency.** Hypothesis was that one
  shared symbols array (vs 100 x ~19 KB arrays) would keep the decode tail
  cache-resident. Measured: `--shared-codebook` at 1M x 100 costs **875 -> 1380
  bits/key (+58%)** because the pooled code lengthens per-column codes
  (13.3 vs 8.8 avg bits/symbol), and the in-process query was ~5.1 us — slower,
  since the arena grew 1.5x. Dead as a latency lever on this data.

- **BUG (must fix before merging the branch): shared-codebook round-trip.**
  A `--shared-codebook --prefilter auto` index passes its in-memory
  correctness checks, but the *reloaded* index returns wrong values
  (`results/csfs/final_m100_sharedcb`). The C++-side `MultisetCsf::load` on the
  same directory allocates unboundedly and gets OOM-killed (likely misparsing
  the per-column filter stream — possibly missing polymorphic filter
  registration in binaries that don't link the filter TUs). The python test
  suite covers shared_codebook and filters separately but not together, which
  is how this slipped through.

## Round 2: toward 10x on the branch point (same day)

Target: 10x on `exp/interleaved-layout`'s measured 4117 ns at 1M x 100, i.e.
~412 ns. Changes, in order landed:

1. **Multiply-fold position derivation** (`signatureToEquation`). The 128-bit
   key signature is already full-strength SpookyHash; deriving the three
   equation positions per column needs decorrelation, not avalanche, so three
   wyhash-style multiply-folds replace the 12-round ShortMix. Construction uses
   the identical derivation (this changes the index format; seed retry remains
   the solvability fallback). Build time unchanged at 1M x 100 (40.1 s), i.e.
   no retry inflation; all 111 C++ / 76 python tests pass.
2. **10k-bucket geometry** (`CARAMEL_BUCKET_EQUATIONS` / `CARAMEL_MAX_BUCKET_DIVISOR`
   env knobs, re-added). At 1M x 100 this is the N/100 clamp ceiling. Each
   per-(bucket, col) range drops to ~751 bits so a column's probes sit in 1-2
   cache lines. Side effects all favorable except size: build 40 -> 17.8 s,
   peak RSS 3.6 -> 2.3 GB, size 109.4 -> 118.1 MB (+8%). 20k buckets measured
   869/744 ns for +18% size -- not worth it; 10k is the spot.
3. **Three-stage decode pipeline + packed query cache.** queryAll now runs
   issue(k+1) -> gather+index+symbol-prefetch(k) -> symbol-load(k); the
   per-cell query cache is 8 bytes (u32 word offset + vars<<7|seed) instead of
   a 16-byte pointer struct -- at 10k buckets that array is 8 MB and streamed
   per query, so entry size is bandwidth; and per-column hot pointers
   (limit/bias/symbols) live in one flat array instead of behind codebook
   shared_ptrs. The symbols[idx] load measured ~1.8 ns/col before prefetching.
4. **queryInto** out-parameter API (skips the per-call vector alloc, ~60 ns).

Result at 1M x 100 (10k buckets): **900 ns single / 779 batch** -- 4.6x / 5.3x
vs the branch point. At 1M x 20 (default geometry is already at the clamp):
224 / 157 ns, or 3.2x / 4.6x vs the 715 ns branch point.

Scorecard vs 10x = 412 ns: **not there; at ~5.3x** (batch). The remaining
~1.9x is per-column compute: positions ~1.5 ns, cell load ~0.5, arena gather
~2.5, decode index ~1.5, symbol ~0.5, store/loop ~1 = ~8 ns/col. Paths that
could close it: NEON across columns (limited -- ARM has no 64-bit vector
multiply, gathers don't vectorize), length-limited codes shrinking the decode
loop 19 -> ~13 iterations, and thread-parallel columns (latency, not
throughput). Throughput 10x is already trivial with cores. Note the 4117
baseline is a 1M-scale number; at 50-100M the pre-change structure degraded
into DRAM/TLB misses while the small-bucket layout keeps the per-query working
set at ~2.5 KB + metadata, so the multiple at the scale that matters should be
larger -- needs a 10M-scale A/B to state honestly.

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
