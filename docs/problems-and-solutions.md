# Problems and Solutions — `p_suche` C++ Project

This document records the technical problems encountered while building the median-network search application, its test suite, benchmarks, CSV reporting, and median-of-medians module — together with the solutions applied.

---

## Table of contents

1. [Project setup and Makefile](#1-project-setup-and-makefile)
2. [Config file parsing](#2-config-file-parsing)
3. [Network file parser](#3-network-file-parser)
4. [Exhaustive search limitations](#4-exhaustive-search-limitations)
5. [Console and file output](#5-console-and-file-output)
6. [Test suite](#6-test-suite)
7. [Benchmark timing](#7-benchmark-timing)
8. [Parallel validation overhead](#8-parallel-validation-overhead)
9. [CSV result recording](#9-csv-result-recording)
10. [Project file structure](#10-project-file-structure)
11. [Compilation warnings and errors](#11-compilation-warnings-and-errors)
12. [Median-of-medians module](#12-median-of-medians-module)
13. [1M random list testing](#13-1m-random-list-testing)
14. [Makefile targets](#14-makefile-targets)
15. [Debugging attempts that failed](#15-debugging-attempts-that-failed)
16. [Lessons learned](#16-lessons-learned)
17. [How to reproduce the working state](#17-how-to-reproduce-the-working-state)

---

## 1. Project setup and Makefile

### Problem
The PVA assignment requires a C/C++ application built with a Makefile (or equivalent project file), runnable from the command line, with release-mode compilation for meaningful benchmarks.

### Solution
- Created `cpp/` with `include/`, `src/`, `tests/`, `build/`
- `make` builds `build/p_suche`; `make test` builds and runs `build/run_tests`
- **Release is the default** (`-O3 -DNDEBUG`); use `make BUILD=debug` for debugging
- Library objects (`LIB_OBJS`) are shared between the app and test binary; `main.cpp` is excluded from tests

### Problem (follow-up)
Initially there was no separation between app code, test helpers, and generated output — files accumulated in the project root.

### Solution
See [§10 Project file structure](#10-project-file-structure).

---

## 2. Config file parsing

### Problem
The assignment workflow was changed from CLI arguments (`-n 5 --goal swaps`) to a single `config.txt` file the user edits before running.

### Solution
- Added `config.hpp` / `config.cpp` with `loadConfig()`
- `main` reads only `config.txt` (no alternate path argument)
- Keys: `n`, `goal`, `output`, `threads`, `max_stages`, `max_swaps`
- `printUsage()` documents the format when parsing fails

---

### Problem: inline `#` comments broke integer parsing

**Symptom:** Running `./build/p_suche` failed with:
```text
Invalid value for threads at line 13
```

**Cause:** `config.txt` contained:
```ini
threads = 0  # use all cores
```
The parser passed `"0  # use all cores"` to `parsePositiveInt`, which rejected it.

**Solution:** Added `stripInlineComment()` in `config.cpp` — everything after `#` on a value line is removed before parsing.

---

### Problem: `threads = 0` rejected as invalid

**Symptom:** Same error as above, even without inline comments.

**Cause:** `parsePositiveInt` requires `parsed > 0`, but `0` means “use all hardware threads”.

**Solution:** Special case in config loader:
```cpp
if (value == "0") {
    config.threads = 0;
}
```

---

### Problem: accidental deletion of `trim()` during refactor

**Symptom:** Compile error — `trim` was called from `stripInlineComment` but not defined.

**Cause:** A search-and-replace replaced the start of `trim()` with `stripInlineComment()`, leaving orphaned function body lines.

**Solution:** Restored both functions: `trim()` first, then `stripInlineComment()` calling `trim()`.

---

## 3. Network file parser

### Problem
`readNetworkFile` failed on valid network files. Stages like `[0,1],[2,3]` were parsed by splitting on **every** comma, so `[0,1]` was broken into invalid tokens `[0` and `1]`.

### Symptoms
- `make test` failed on `testReadFixtureNetworks` and `testWriteAndReadSingleNetwork`
- Debug output: `Invalid compare-swap token: [0`

### Solution
Rewrote `parseStageLine` in `src/network_io.cpp` to scan for `[...]` bracket groups instead of naive comma splitting. Each compare-swap token is parsed as a single unit.

---

## 4. Exhaustive search limitations

### Problem
The v1 search (`src/search.cpp`) enumerates all candidate networks up to `max_stages` / `max_swaps` and validates each on all input permutations. This is correct but does not scale.

### Symptoms
| n | Goal   | Time (release) | Optimal networks found |
|---|--------|----------------|------------------------|
| 3 | swaps  | < 1 ms         | 6                      |
| 5 | swaps  | ~16 s          | 1,350                  |
| 5 | stages | ~6 s           | 39,000                 |

### Cause
Combinatorial explosion of stage combinations and network orderings. Many networks are symmetric variants of the same optimum.

### Current approach
- `sortAndUniqueNetworks()` deduplicates lexicographically
- Search stops at the first (minimal) swap/stage count that yields valid networks
- `max_stages` / `max_swaps` in `config.txt` cap the search

### Known limitation (not yet fixed)
`filterValidNetworks()` distributes **candidates** across threads but calls `isValidMedianNetwork(candidates[i], n, 1)` — validation inside each worker is still single-threaded. Parallelism helps only across different candidates, not within one network’s permutation check.

### Future work (for the assignment report)
- Branch-and-bound or evolutionary search (e.g. SorterHunter-style)
- OpenMP on the HPC cluster
- Canonical form to reduce duplicate optima

---

## 5. Console and file output

### Problem: terminal flooded with thousands of networks

**Symptom:** Running `n=5` printed 1,350 full networks to stdout (~10,000 lines).

**Solution:**
- `writeNetworksFile()` writes all networks to one file, separated by `---`
- Console shows at most 3 preview networks plus a “… and N more” line

---

### Problem: one file per optimal network

**Symptom:** Early version wrote `median_network.txt.1`, `.2`, … `.1350`.

**Solution:** Single output file with `---` separators between networks.

---

### Problem: generated output in project root

**Symptom:** `median_network.txt` appeared next to `config.txt` and source files.

**Solution:** Default output path changed to `build/median_network.txt`; `build/` is gitignored.

---

## 6. Test suite

### Problem
No automated way to verify search results, I/O, config parsing, or performance.

### Solution
- Lightweight test runner in `tests/run_tests.cpp` with `CHECK` / `CHECK_EQ` and `runTest()` timing
- Fixture files under `tests/fixtures/config/` and `tests/fixtures/networks/`
- `verifyMedianNetwork()` — format check + median correctness on all permutations
- `make test` runs all unit tests, then quick benchmarks
- Tests always write CSV even on failure (for debugging)

### Test coverage (current)
| Area            | Tests |
|-----------------|-------|
| Network apply   | `testApplyCompareSwap`, `testApplyNetwork` |
| Format / I/O    | `testFormatNetwork`, `testWriteAndReadSingleNetwork`, `testReadFixtureNetworks` |
| Validator       | `testValidatorRejectsInvalidNetworks`, `testValidatorMatchesParallelCheck` |
| Config          | `testConfigValidFixture`, `testConfigInvalidFixtures` |
| Search          | `testSearchN3Swaps`, `testSearchN3Stages`, `testSearchOutputRoundTrip`, `testSearchResultMatchesOptimumMetric` |
| Median-of-medians | `testMedianOfMediansMatchesSort`, `testMedianOnRandomList1e6` |

---

## 7. Benchmark timing

### Problem
Validation on small `n` (e.g. `n=3`) finished in sub-microsecond time. A single `measureMs()` call often printed `0.00 ms`.

### Solution
- `measureRepeatedMs()` — repeat until at least 5 ms total, then average
- `printBenchmark()` switches units: µs, ms, or seconds depending on magnitude
- Split into **quick** (`make test`: n=3 search + n=1000 median) and **full** (`make bench`: n=5 search, n=10000 median, optional n=1M)

---

### Problem: full benchmarks made `make test` too slow

**Symptom:** `make test` took ~20+ seconds because n=5 search ran every time.

**Solution:**
- `make test` → quick benchmarks only
- `make bench` → sets `P_SUCHE_BENCH_ONLY=1` and runs full suite including slow n=5 search

---

## 8. Parallel validation overhead

### Problem
On small inputs, `validation n=3 (all threads)` was much **slower** than single-thread (e.g. ~0.01× “speedup”).

### Cause
Thread creation and synchronization overhead dominates when work per thread is tiny (only 6 permutations for `n=3`).

### Solution
Not a code bug — expected behavior. Benchmarks record the ratio honestly in CSV for the assignment report (speedup *S* and efficiency *E* discussion). Parallel validation becomes more useful as `n` grows (e.g. `n=5`: 120× vs 0.15× speedup observed).

---

## 9. CSV result recording

### Problem
Needed persistent test/benchmark data for the written report (Excel, plots, HPC comparison tables).

### Solution
- `tests/support/test_report.hpp` and `test_report.cpp`
- `TestRecord` and `BenchmarkRecord` collected during `runTest()` and `logAndRecordBenchmark()`
- `writeReportCsvFiles()` at end of every test/bench run

### Output files
| File | Columns |
|------|---------|
| `build/test_results.csv` | timestamp, build, threads, test_name, status, duration_ms |
| `build/benchmark_results.csv` | timestamp, build, threads, benchmark_name, duration_ms, speedup, networks_found, optimum, detail |

### Problem: compiler warnings on partial struct initialization

**Symptom:**
```text
warning: missing field 'detail' initializer [-Wmissing-field-initializers]
```

**Solution:** Fully initialize `BenchmarkRecord` fields; use `-1` for unused optional numeric columns and `""` for empty detail.

---

## 10. Project file structure

### Problem
Mixed concerns: test helpers beside fixtures, generated CSV/network files in root, unclear include paths for tests.

### Solution — final layout
```text
cpp/
├── .gitignore
├── Makefile
├── config.txt
├── include/                 # app headers
├── src/                     # app implementation
├── scripts/
│   └── generate_random_list.py
├── tests/
│   ├── run_tests.cpp
│   ├── support/             # test-only code (test_report)
│   └── fixtures/
│       ├── config/
│       └── networks/
└── build/                   # binaries, CSV, outputs (gitignored)
```

### Makefile changes
- `TEST_INCLUDES := -Iinclude -Itests/support` so `run_tests.cpp` can `#include "test_report.hpp"`
- `TEST_SRCS` globs `tests/support/*.cpp` automatically

---

## 11. Compilation warnings and errors

| Issue | Symptom | Solution |
|-------|---------|----------|
| Undefined `BUILD_MODE` in benchmarks | Compile error in early `run_tests.cpp` | Use `#ifdef NDEBUG` → `"release"` / `"debug"` |
| Duplicate `#include <vector>` | Warning in `network.hpp` | Remove duplicate include |
| `trim()` accidentally deleted | `stripInlineComment` calls undefined `trim` | Restore `trim()` function body |
| `BenchmarkRecord` partial init | `-Wmissing-field-initializers` | Initialize all fields explicitly |
| `g++` heredoc from stdin | `clang++: -E or -x required` when piping code | Always use a real `.cpp` file on disk |
| Test binary missing `test_report` symbols | Link error | Add `tests/support/test_report.cpp` to `TEST_SRCS` |

---

## 12. Median-of-medians module

Port of a Python median-of-medians algorithm to C++ (`src/median.cpp`, `include/median.hpp`) for comparison with sort-based median and a parallel variant.

### 12.1 Mutual recursion and stack overflow

**Symptom:** Segfault around 350k–600k elements; worked at 300k, crashed at 380k+.

**Cause:** Early versions had `medianOfMediansImpl` and `selectPivot` calling each other recursively. Combined with deep partition recursion, the C++ call stack overflowed.

**Attempted fixes:**
- Iterative outer `while` loop for the main partition
- Explicit `std::vector` job stacks for pivot selection
- Larger linker stack (`-Wl,-stack_size,...`) — did not fix root cause

---

### 12.2 Use-after-move bugs

**Symptom:** Wrong median values or immediate crash.

**Cause:**
```cpp
momSelect(std::move(medians), medians.size() / 2);  // BUG: size after move
```

**Solution:** Save `medianIndex = medians.size() / 2` **before** `std::move(medians)`.

---

### 12.3 Infinite loops during partition

**Symptom:** `medianOfMedians` on 1M elements hung for minutes.

**Cause:** When the pivot did not split the array (all elements in `low`), the loop continued with the same-sized array forever.

**Attempted fix:** Fallback to `medianBySort()` when `low.size() >= values.size()` — stopped hangs but produced **wrong** medians (e.g. 5000231 vs 5000184).

---

### 12.4 Empty `high` partition

**Symptom:** Segfault when `index > k` but `high` was empty (many duplicates equal to pivot).

**Solution:** If `high.empty()` after partition, return `pivot` immediately.

---

### 12.5 Wrong results from broken state machines

Several explicit-stack implementations had incorrect parent/child resume logic, returning garbage or mismatched values. Abandoned in favor of simpler structure below.

---

### Final solution (current code)

```text
medianOfMedians / medianOfMediansParallel
  └─ if n > 500'000 → medianBySort (safe fallback)
  └─ else nthElementMom (iterative outer loop)
        └─ pivot via momSelect (recursive only on medians array, bounded depth)
        └─ parallel sublist extraction when n ≥ 50'000
```

| Constant | Value | Purpose |
|----------|-------|---------|
| `kMomDirectMaxSize` | 500,000 | Above this, use sort to avoid stack overflow |
| `kParallelThreshold` | 50,000 | Parallel sublist median extraction |

---

## 13. 1M random list testing

### Problem
Need a large input file to measure median algorithms at assignment-relevant scale.

### Solution
- `cpp/scripts/generate_random_list.py` — writes one integer per line, seed 42
- Output: `build/random_list_1e6.txt`
- `loadIntListFromFile()` in `run_tests.cpp`
- `testMedianOnRandomList1e6` — verifies sort median on full 1M; verifies mom on 500k prefix
- `make median-1m` — `P_SUCHE_MEDIAN_1M=1 ./build/run_tests`

### Problem: `make clean` deletes the random list

**Cause:** File lives in `build/`, removed by `rm -rf build`.

**Solution:** Regenerate before large benchmarks:
```bash
python3 scripts/generate_random_list.py -n 1000000 -o build/random_list_1e6.txt
```

### Observed results (release, ~10 threads)

| Benchmark | Time | Notes |
|-----------|------|-------|
| Sort, n=1M | ~16.5 ms | Ground truth median: **5,000,184** |
| Mom, n=1M | ~16 ms | Uses sort fallback (n > 500k) |
| Mom sequential, 500k prefix | ~21 ms | Real mom algorithm |
| Mom parallel, 500k prefix | ~16 ms | ~1.29× vs sequential mom |

---

## 14. Makefile targets

| Target | What it runs |
|--------|----------------|
| `make` / `make all` | Build `build/p_suche` |
| `make run` | Build and run app (`config.txt`) |
| `make test` | Unit tests + quick benchmarks + CSV |
| `make bench` | Full benchmarks only (slow) + CSV |
| `make median-1m` | 1M median benchmarks only + CSV |
| `make clean` | Remove entire `build/` |
| `make BUILD=debug` | Debug flags (`-g -O0`) |

### Environment variables (test runner)

| Variable | Effect |
|----------|--------|
| `P_SUCHE_BENCH_ONLY=1` | Skip unit tests; run all benchmarks |
| `P_SUCHE_MEDIAN_1M=1` | Run only the 1M median benchmark block |

---

## 15. Debugging attempts that failed

| Attempt | Result |
|---------|--------|
| `lldb` backtrace on segfault | Session aborted before useful trace |
| Long-running mom on 1M without fallback | Hung 5+ minutes — infinite partition loop |
| Sort fallback on degenerate partition | Fixed hang but wrong median |
| Increasing stack size only | Did not fix mutual recursion overflow |
| Parallel validation for n=3 | Measurable slowdown, not a bug |

---

## 16. Lessons learned

1. **Bracket-aware parsing** is required for network stage lines with comma-separated `[a,b]` tokens.
2. **Config parsers** must strip inline comments and treat `0` as a special sentinel for “auto”.
3. **Micro-benchmarks** need repeated measurement; single samples lie for sub-millisecond work.
4. **Parallelism is not free** — measure on realistic input sizes before claiming speedup.
5. **Exhaustive search** is fine for v1 / small `n`; document combinatorial limits honestly.
6. **Recursive mom in C++** needs iterative outer loops + bounded recursive pivot selection.
7. **`std::move` invalidates** the source — never read `.size()` on a moved-from vector.
8. **Partition loops** must handle “all elements equal to pivot” (`high.empty()`).
9. **Large-n fallbacks** (sort above 500k) are a pragmatic trade-off for in-process safety.
10. **Generated artifacts belong in `build/`** — regenerate after `make clean`.
11. **Separate quick and full benchmarks** so CI/local `make test` stays fast.

---

## 17. How to reproduce the working state

```bash
cd cpp

# Optional: generate 1M test data
python3 scripts/generate_random_list.py

# Build and run tests (fast)
make test

# Full benchmarks (slow: includes n=5 search, ~20 s)
make bench

# 1M median comparison only
make median-1m

# Run the median-network search app
make run
# edit config.txt first
```

**Expected:**
- All unit tests pass
- CSV files written under `build/`
- Median on 1M list = **5,000,184** (via sort)
- Optimal n=5 swap network uses **7** compare-swaps across **5** stages
