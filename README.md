# Parallel-search

Parallele Suche nach **beweisbar optimalen** Median-Netzwerken (PVA HS Osnabrück).

Algorithmus: 0/1-Prinzip, bitparallele Validierung, IDDFS, **TBB**-Parallelisierung, Pruning (Parberry/Sekanina-Kontext).

## Quick start

```bash
cd cpp
make
make test
./build/p_suche --n 5 --opt size --compare --threads 8 --first --out build/median_network.txt
```

## CLI

```
./build/p_suche --n <odd> --opt size|depth [--threads N] [--out file]
                [--sequential] [--compare] [--first]
```

| Flag | Meaning |
|------|---------|
| `--n` | Channels (odd, 3..19) |
| `--opt size` | Minimize compare-swaps |
| `--opt depth` | Minimize stages |
| `--threads` | TBB threads (0 = hardware default) |
| `--sequential` | Single-threaded search |
| `--compare` | Sequential + TBB in one run; prints Speedup S and Efficiency E |
| `--first` | Stop after first optimum |
| `--out` | Output file |

## HPC (~/PVA on cluster)

```bash
sbatch scripts/hpc/smoke_test.slurm
sbatch scripts/hpc/bench.slurm
```

## Docs

- `docs/project-plan.md`
- `docs/performance-report.md`
- `docs/PVA_Themenstellung_Wdh_SS2026_A01.pdf`
