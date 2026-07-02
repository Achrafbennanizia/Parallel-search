# Performance Report — `p_suche`

General log of changes made to **improve runtime or benchmark results**.  
Raw numbers: `cpp/results/benchmark_results.csv` (cumulative, append-only).

---

## Changelog (newest first)

| Date | Area | Change | Expected effect | Measured result |
|------|------|--------|-----------------|-----------------|
| 2026-07-02 | parallel | OpenMP removed; TBB only via `tbb::parallel_for` | Simpler parallel layer | `make test` |
| 2026-07-02 | benchmark | `--compare` runs sequential + TBB in one invocation | Speedup S & Efficiency E in one run | n=5 ~1.66× local |
| 2026-06-14 | parallel | Rank-based permutation validation + `std::thread` / optional OpenMP (`OPENMP=1`) | Speedup on n≥5 validation | replaced by TBB/thread backends |
| 2026-06-14 | search | Parallel `filterValidNetworks`; `threads` in config | Faster exhaustive search | n=5 swaps: **26.3 s → 15.3 s** (10 threads, ~1.7×) |
| 2026-06-14 | evolution | Evolutionary search (`evolution.cpp`), `search = auto\|exhaustive\|evolutionary` | Scales to n≥7 | `testEvolutionN3FindsOptimum` |
| 2026-06-29 | types | `SampleValue` = `uint32_t` for lists/network values | Same size as `int32`; correct range, no speed gain vs `int` | Equivalent to prior `int` benchmarks |
| 2026-06-29 | types | Evaluated `u16` / `u64` | `u16` only if values ≤ 65535; `u64` uses 2× memory | See **Fixed-width integers** below |
| 2026-06-29 | median | Sort-based median only (`medianBySort`) | Lower constant factors than mom on practical n | 1M ~15 ms; 1B ~16.8 s (release) |
| 2026-06-29 | build | Release default (`-O3 -DNDEBUG`) | Much faster hot paths | Required for meaningful benchmarks |
| 2026-06-29 | benchmarks | `measureRepeatedMs()` for sub-ms work | Stable µs timings | Validation n=3 no longer shows 0.00 ms |
| 2026-06-29 | benchmarks | Quick vs full split (`make test` / `make bench`) | Fast CI; slow n=5 only on demand | `make test` stays under ~1 s (+ 1M median) |
| 2026-06-29 | benchmarks | Cumulative CSV in `results/` | Track trends over runs | `results/benchmark_results.csv` |

---

## Details

### Parallel backends (`include/parallel.hpp`)

- **Default:** `parallelFor` uses `std::thread` chunking (`backend=thread`).
- **TBB:** `make TBB=1` links Intel oneTBB; same loops use `tbb::parallel_for`.
- **Used in:** permutation validation, candidate filtering, evolution fitness.
- **Compare:** `make test-parallel` (thread) then `make test-tbb` (TBB) — benchmark names include `[thread]` or `[tbb]`.

### Parallel validation & search (Phase 1)

- **What:** `isValidMedianNetwork(network, n, threads)` partitions `n!` permutations by rank; `filterValidNetworks` splits candidates across `parallelFor`. Config key `threads` (0 = hardware concurrency).
- **Verify:** `make test`, `make test-parallel`, `make test-tbb`.

### Evolutionary search (Phase 2)

- **What:** `evolveMedianNetworks()` — random networks, crossover/mutation, parallel population evaluation. `search = auto` uses exhaustive for `n ≤ 5`, evolution for `n ≥ 7`.
- **Verify:** `testEvolutionN3FindsOptimum`; set `search = evolutionary` in `config.txt` for larger n.

### Release build (`Makefile`)

- **What:** `BUILD=release` is default: `-O3 -DNDEBUG`.
- **Why:** Debug (`-O0`) inflates sort and search times; assignment requires release for benchmarks.
- **Verify:** `make test` vs `make BUILD=debug test`.

### Sort-based median (`src/median.cpp`)

- **What:** Replaced median-of-medians with `std::sort` + middle index.
- **Why:** Mom had recursion, extra allocations, and fallbacks; sort won on n=1000 (~0.008 ms vs ~0.04–0.07 ms mom in CSV).
- **Verify:** `make test` (1M), `make test-1b` (1B).

### Fixed-width integers (`SampleValue` = `uint32_t`)

- **u32:** Same 4 bytes as `int` on this platform → **no meaningful speedup**; adopted for correct typing (values 0..10_000_000).
- **u16:** Would halve memory (1B → ~1.85 GiB) and can speed sort via cache — **only if** `max ≤ 65535` in the generator.
- **u64:** 8 bytes/element → **slower** for 1B (~7.4 GiB); do not use for current data.

To try u16: regenerate with `--max 65535 --width 16` (not implemented yet; would need binary format v2).

### Binary 1B dataset (`scripts/generate_random_list.py`)

- **What:** `--binary` writes chunked random ints; loader in `tests/run_tests.cpp`.
- **Why:** Text (1 int/line) is impractical at 10⁹ scale; binary avoids parsing overhead.
- **Verify:** `make data-1b` then `make test-1b`.

### Benchmark methodology (`tests/run_tests.cpp`)

- **What:** Repeated timing until ≥5 ms total for micro-benchmarks; unit scaling (µs/ms/s) in output.
- **Why:** Single-sample sub-ms measurements were noise.
- **Verify:** `make test` → `BENCH` lines + CSV append.

---

## Reference results (release)

| Benchmark | Approx. time | Notes |
|-----------|--------------|-------|
| median sort n=1,000,000 | ~15 ms | `make test` |
| median sort n=1,000,000,000 | ~16.8 s | `make test-1b`, median = 5,000,192 |
| search n=5 goal=swaps (1 thread) | ~26 s | `make test-parallel` |
| search n=5 goal=swaps (10 threads) | ~15 s | ~1.7× speedup (candidate parallelism) |

---

## How to add a new entry

1. Implement the performance change.
2. Run the relevant benchmark (`make test`, `make test-1b`, or `make bench`).
3. Add a row to the changelog table above (newest first).
4. Add a **Details** subsection if the change needs more than one line.
