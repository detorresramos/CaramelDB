# RecSys 2026 Paper: Plan of Attack

Planned changes to take `main.tex` from ~4 pages of mostly-stub content to a
complete 8-page submission. Grouped by type. Prioritized ordering at the end.

---

## 1. Paper writing (section-by-section)

### 1.1 Empty stubs to fill

- **Methodology (§3).** Currently `"Describe how the system works"`. Becomes
  the System Overview:
  - Matrix view: keys → rows, ranked positions → columns.
  - Multi-CSF architecture: one CSF per column; query = concatenate $m$ single-CSF
    lookups.
  - Decision-tree framing of the three RQs: (i) share codebook? (ii) permute?
    (iii) ragged? — each answered by one of §4, §5, §6.
  - Mention the prefilter (AutoFilter / MCV extraction) used across all
    experiments.
  - Figure F1 (architecture diagram).

- **Experiments (§7).** Currently `"Present experiment results"`. Full section:
  - §7.1 Setup: hardware (96 GB EC2 box), datasets, metrics (bits/key, query ns,
    build s), baselines.
  - §7.2 Synthetic experiments validating theory (from §2 of this plan).
  - §7.3 Amazon experiments with baselines.
  - §7.4 Pareto plot (headline figure).

- **Related Work (§8).** Currently `"Discuss prior work..."`. Prose over the
  existing bib entries:
  - Succinct data structures (Succinct/NSDI'15, FM-index).
  - Compressed KV stores (IndeedMPH, MPH-based stores, RocksDB with compression).
  - CSF lineage (Hreinsson/Krøyer/Pagh, Belazzougui–Venturini, Genuzio–Vigna).
  - Result caching in recsys / search (ROSE).
  - Integer compression for list values (Scholer et al., Williams–Zobel).

- **Conclusion (§9).** Currently `"Wrap it up..."`. 1 paragraph summary +
  1 paragraph limitations / future work (dynamic updates, distributed sharding,
  GPU-friendly queries, non-integer value types).

### 1.2 Section expansions

- **Background (§2).** Ends mid-thought after the codebook cost line. Extend
  with ~0.5 page on:
  - Length-limited canonical Huffman (why $\Lmax$ matters, Abu-Mostafa bound).
  - Chunk-based construction at scale (why Caramel uses $2^\ell$-key chunks).
  - Prefilter / AutoFilter: removing the most-common value out-of-band
    to shrink the bit array. Materially reduces space and is used in every
    experiment.

- **Codebook Sharing (§4).** Add a new proposition that sharpens the story
  significantly (and that is currently missing):

  > **Proposition (Permute-vs-Share collapse).** The pooled distribution
  > $\bar p$ is invariant under within-row permutations of $A$. Consequently
  > $\mathrm{Space}(\mathbf{S})$ is unchanged by any row-level permutation,
  > while $\mathrm{Space}(\mathbf{P})$ is monotonically decreased by
  > permutation that reduces $\sum_j H(p_j)$. Therefore, across the 2×2
  > grid $\{\text{private, shared}\} \times \{\text{no-permute, permute}\}$,
  > the configuration `Shared + Permute` is never strictly better than
  > `Private + Permute`, and the grid effectively collapses to three
  > strategies: `Private`, `Shared`, and `Private + Permute`.

  This justifies not running the 4th strategy empirically, matches the grid
  we are actually running, and turns two separate design choices into a single
  decision tree.

- **Row Permutation (§5).** Reorder and add subsection headers:
  - §5.1 Problem formulation (define $F(\sigma)$, restate objective).
  - §5.2 Hardness (existing NP-completeness proof, now motivating).
  - §5.3 CaramelSort heuristic (existing algorithm).
  - Add a one-line transition between §5.2 and §5.3:
    *"Since optimizing $F$ is intractable, we turn to a principled heuristic."*

- **Ragged Arrays (§6).** New short section (currently missing entirely from
  `main.tex`'s `\input` list, actually — check: it's not there). Two framings,
  pick based on what the synthetic data shows:
  - *(a)* Empirical exploration. Present three encodings (padding,
    length-column + fixed CSF, packed CSF with stop symbol) with a results
    table on synthetic ragged data of varying length variance.
  - *(b)* Mini-theorem. Stop-symbol overhead is $\approx \log_2|\Sigma| + 1$
    bits per stop; length-column overhead is $\lceil\log_2 \Lmax\rceil$ bits
    per row. Closed-form crossover rule analogous to Prop. 4.1.

  Preferred: try (b) first; fall back to (a) if the algebra doesn't close.

### 1.3 Intro / abstract fixes

- Fill the `XXX` memory-reduction number in the abstract once the Amazon grid
  finishes.
- Expand the 4 bare contribution bullets in the intro to 1–2 sentence claims
  that name the result (e.g., *"We prove a parameter-free decision criterion
  (Prop. 4.1) for when a single shared codebook beats per-column codebooks..."*).

### 1.4 Add `\input{ragged}` and `\input{permute-share}` to `main.tex`

Currently `main.tex` only includes 9 files; ragged and the new share-vs-permute
discussion need homes.

---

## 2. Synthetic experiments (theory validation + tradeoff clarity)

Purpose: answer the three RQs in a controlled setting where we can ablate a
single knob and overlay theoretical predictions.

### 2.1 Codebook sharing crossover (validates Prop. 4.1)

- Data generator: $m$ column distributions, each Zipfian over $|\Sigma|$ values,
  with a tunable per-column dispersion parameter that controls
  $\mathrm{KL}(p_j \| \bar p)$.
- Fixed: $n = 10^6$, $|\Sigma| \in \{10^3, 10^4, 10^5\}$, $m \in \{10, 50, 100\}$.
- Sweep: dispersion knob, measuring total bits/key for Private vs Shared at
  each setting.
- Overlay: theoretical curve from Prop. 4.1.
- **Deliverable**: F2. "Theory tracks empirics within X% across Y orders of
  magnitude of KL."

### 2.2 Permute-vs-share confirmation (validates new proposition)

- Same synthetic setup as 2.1.
- Measure all four strategies: `P`, `S`, `P+perm`, `S+perm`.
- Confirm: `S+perm` ≈ `S`; `P+perm` ≤ `P`; `S+perm` never strictly beats
  `P+perm` when private wins post-permute.
- **Deliverable**: bar chart showing the 2×2 collapses to 3.

### 2.3 CaramelSort convergence and quality

- Data generator: ragged or fixed $m$ with controlled per-column skew.
- Plot: $\sum_j H(p_j)$ vs iteration.
- Compare against: frequency-sort only (no refinement), random permutation,
  no permutation, and (if tractable) the optimal for a small instance.
- Sweep $m$, $|\Sigma|$, Zipf exponent.
- **Deliverable**: F3 convergence plot + F4 column-entropy heatmap pre/post.

### 2.4 Ragged encoding tradeoff

- Data generator: ragged lists with controllable $\mu$ and $\sigma$ of length.
- Encodings compared:
  - Padding to $\max_i |v_i|$ + fixed-length multiset CSF.
  - Length column (a small CSF) + fixed-length multiset CSF over padded data.
  - Packed CSF (single CSF over concatenated Huffman codes + stop symbol).
- Report bits/key and query ns.
- **Deliverable**: F5 + a results table + a practical rule-of-thumb.

---

## 3. Amazon experiments with baselines

### 3.1 Current grid (already running)

- Datasets: `asin_clicks`, `query_freq`.
- $N = 10^8$. $M \in \{10, 100\}$ for the paper body; $\{25, 50\}$ in an
  appendix if space allows.
- Strategies: `Column` (Private), `Column+Permute` (Private+Permute),
  `Column+SharedCB` (Shared). `Shared+Permute` omitted per Prop. above.
- Metrics: bits/key, permute-s, build-s, query ns.
- Output: `scripts/empirical_grid_results.json`, rendered into
  `artifacts/multiset-benchmark-empirical.md`.

**Note (serialization bug correction):** The first wave of runs was affected by
a shared-codebook serialization bug — the codebook was duplicated per column
file (cereal's shared_ptr tracking doesn't cross archives, and the multiset
save writes each column to a separate archive). Fixed in commit `c3d11af`;
retroactive correction applied via `scripts/retroactive_shared_cb_size.py`
(output: `scripts/empirical_grid_results_corrected.json`).

First corrected numbers for asin_clicks $M = 100$:

| Strategy | bits/key |
| :--- | ---: |
| Column (Private) | 2643.83 |
| Column+SharedCB (corrected) | **2384.86** |
| Column+Permute | **1997.99** |

Shared beats Private by ~260 b/k; Permute beats both — matching Prop. 4.1 and
the permute-vs-share collapse proposition. Future runs after `c3d11af` produce
the correct numbers directly without needing retroactive correction.

### 3.2 Baselines to add

All run on the same `asin_clicks` and `query_freq` data at $N = 10^8$,
$M \in \{10, 100\}$. Ordered by how strongly they pressure-test our claims.

1. **`std::unordered_map<uint32_t, std::vector<uint32_t>>`** (already exposed
   as `UnorderedMapBaseline`). Dense in-memory baseline — upper bound on both
   memory (~24 bytes/entry overhead + 4 bytes per value) and on query speed.
2. **Raw array + MPH.** Minimal perfect hash on keys → dense $N \times M \times 4$
   byte array. Represents "don't compress" — lower bound on space savings, upper
   bound on decode speed. Uses the existing `permute_uint32` MPH primitive.
3. **Packed CSF** (already in codebase as `strategy="packed"`). Ablates the
   per-column decomposition: one CSF over concatenated Huffman codes with stop
   symbols. Tests whether column decomposition is actually the right choice at
   scale.
4. **RocksDB** with default Snappy / LZ4 compression. Common embedded-KV
   baseline; stores key → serialized list. Likely much larger than Caramel but
   a standard comparison point.
5. **IndeedMPH** (optional). MPH-based compact KV store. Designed for a similar
   problem but not list-valued; may require adaptation. Include if time permits.
6. **Succinct** (Agarwal et al., NSDI'15) (optional). General compressed-KV
   with query support. Construction may not scale to $10^8$; if infeasible,
   document in limitations and skip.

### 3.3 Headline Pareto plot

- x-axis: bits/key.
- y-axis: query latency (ns).
- Points: all Caramel strategies × datasets × $M$ values + every baseline.
- Pareto frontier highlighted.
- **Deliverable**: F6 — the single most important figure in the paper.

---

## 4. Figures

| ID | Figure | Section | Source |
|----|--------|---------|--------|
| F1 | System architecture diagram | §3 | TikZ / hand-drawn |
| F2 | Codebook-sharing crossover (theory vs empirics) | §4 or §7.2 | Synthetic 2.1 |
| F3 | CaramelSort convergence | §5.3 or §7.2 | Synthetic 2.3 |
| F4 | Column entropy heatmap pre/post permute | §5.3 | Synthetic or Amazon |
| F5 | Ragged encoding comparison | §6 or §7.2 | Synthetic 2.4 |
| F6 | Pareto (latency vs bits/key) across all methods | §7.3 | Amazon grid + baselines |
| F7 | (Optional) Build time vs $N$ or $M$ | §7.3 | Amazon |

## 5. Tables

| ID | Table | Section |
|----|-------|---------|
| T1 | Amazon bits/key grid (datasets × $M$ × strategies) | §7.3 |
| T2 | Amazon query latency grid | §7.3 |
| T3 | Amazon construction time grid (permute-s, build-s) | §7.3 |
| T4 | Amazon vs baselines summary (bits/key, ns/q, build-s) | §7.3 |
| T5 | Ragged encoding comparison | §6 |

---

## 6. Prioritized order of attack

Sorted so that high-value + long-pole items start first, low-value + short items
land at the end. The rationale for this order is that we want the slowest-to-run
experiments and the largest writing blocks underway in parallel; small cosmetic
fixes can happen any time in the final days.

**Tier 1 — long poles, blocking, must start now**

1. **Amazon grid completion** *(in progress, days of compute)*. Blocks T1–T3,
   F6, and the abstract `XXX`.
2. **Implement + run Amazon baselines (§3.2)**. Blocks F6 and T4. Engineering:
   1–2 days for unordered_map runner, raw-array+MPH runner, RocksDB wrapper,
   Packed CSF runner. Compute: 1+ day.
3. **Synthetic experiment 2.1 (codebook crossover)** — highest-value theory
   validation. Needs new data generator + benchmark script. 1 day engineering,
   a few hours of compute.
4. **Synthetic experiment 2.3 (CaramelSort convergence + quality)**. Same
   scaffolding as 2.1. Produces F3 and F4. 0.5–1 day.

**Tier 2 — medium writing blocks that unblock transitions**

5. **Write Methodology (§3)** with architecture figure F1. Unblocks transitions
   throughout the paper. ~0.5 day.
6. **Draft Experiments (§7) skeleton** with placeholder tables and captions,
   so numbers slot in as they land. ~0.25 day.
7. **Synthetic experiment 2.2 (permute-vs-share confirmation)** — small, but
   directly confirms the new proposition.
8. **Add Permute-vs-Share Proposition to §4**. Small text, high conceptual
   value. Short formal proof + one-line consequence for the empirical grid.
   ~0.25 day.
9. **Reorder §5 (hardness before algorithm)** with subsection headers and a
   connective sentence. ~0.1 day.

**Tier 3 — background and supporting content**

10. **Synthetic experiment 2.4 + write Ragged §6**. Depends on 2.4 data.
    0.5 day combined.
11. **Expand Background (§2)** with canonical Huffman, chunk-based construction,
    and prefilter. ~0.3 day.
12. **Write Related Work (§8)** from the existing bib entries. ~0.5 day.

**Tier 4 — finishing touches, fast**

13. **Write Conclusion (§9)**. ~0.1 day.
14. **Fix abstract `XXX`** once Amazon grid completes. Minutes.
15. **Expand intro contribution bullets** to 1–2 sentence claims. ~0.1 day.
16. **Final polish**: figure captions, cross-references, bibliography cleanup,
    page-count trimming.

---

## 7. Risks and contingencies

- **Amazon grid OOMs or fails mid-run.** Mitigation: the memory fixes from the
  recent sprint (LUT disable, streaming shared-codebook freq, mmap values,
  in-place permute) are deployed. Resumption supported via
  `empirical_grid_results.json`. Worst case: drop `query_freq M=100` and
  report `M ∈ {10, 25, 50}` plus `asin_clicks M=100`.
- **Succinct / IndeedMPH baselines infeasible at $N = 10^8$.** Mitigation:
  include smaller-$N$ points for them, or cite and skip with a note.
- **Ragged theory doesn't close.** Mitigation: fall back to the empirical
  framing (option (a) in §1.2).
- **Page count overruns.** Mitigation: F7 and the $M \in \{25, 50\}$ rows
  move to appendix; Related Work compressed to half a page.
