# Project Plan — Parallele Suche nach optimalen Median-Netzwerken

PVA Sommersemester 2026 · Abgabe bis **31. August 2026**

---

## 1. Aufgabenverständnis

| Anforderung | Bedeutung |
|-------------|-----------|
| **Median-Netzwerk** | Compare-Swap-Stufen; nach Durchlauf steht der Median auf Kanal `n/2` |
| **Ziel (i)** | Minimale Anzahl Compare-Swap-Einheiten |
| **Ziel (ii)** | Minimale Anzahl Stufen |
| **Stufe** | Menge disjunkter `[x,y]` (kein Kanal doppelt) → parallel ausführbar |
| **Ausgabe** | Stufenweise, komma-getrennt, lexikographische `[x,y]`-Paare |
| **Kern der Note** | **Parallele Suche** — nicht nur korrektes Programm, sondern Parallelisierung nach Vorlesung |
| **Skalierung** | `n` wählen, sodass Laufzeit Sekunden → Minuten → Stunden reicht |

---

## 2. Ist-Stand (bereits implementiert)

```
cpp/
├── config.txt              # Steuerung (n, goal, output, limits)
├── src/search.cpp          # Exhaustive search v1 (serial)
├── src/validator.cpp       # Prüfung aller Permutationen (serial)
├── src/network*.cpp        # Anwenden, Format, Datei-I/O
├── tests/                  # Unit-Tests, Benchmarks, CSV
└── docs/                   # problems-and-solutions, performance-report
```

| Komponente | Status |
|------------|--------|
| Config-Datei statt CLI | ✅ |
| Ausgabeformat `[x,y]` pro Stufe | ✅ |
| Ziele `swaps` / `stages` | ✅ |
| Korrektheit n=3, n=5 | ✅ (exhaustive) |
| Release-Build, Benchmarks, CSV | ✅ |
| Parallele Suche | ❌ (bewusst serial als Baseline) |
| Evolutionäre / intelligente Suche | ❌ |
| HPC-Läufe, Speedup S, Effizienz E | ❌ |
| Projektbericht + Referat | ❌ |

**Grenzen v1:** Exhaustive search explodiert bei n≥5 (z. B. 1 350 Netze bei 7 Swaps; ~16 s). Für größeres `n` unbrauchbar.

---

## 3. Soll-Architektur

```mermaid
flowchart TB
    subgraph input [Eingabe]
        CFG[config.txt]
    end

    subgraph search [Parallele Suche]
        DEC[Dekomposition / Arbeitspakete]
        VAL[Validator: alle Permutationen]
        OPT[Optimierer: swaps oder stages]
    end

    subgraph strategies [Suchstrategien]
        EXH[Exhaustive - kleines n]
        EVOL[Evolutionär - großes n]
        BB[Branch-and-Bound - optional]
    end

    subgraph output [Ausgabe]
        NET[optimale Netzwerk(e)]
        CSV[Benchmarks / Speedup]
    end

    CFG --> DEC
    DEC --> strategies
    strategies --> VAL
    VAL --> OPT
    OPT --> NET
    OPT --> CSV
```

---

## 4. Parallelisierung (Vorlesungsbezug)

Drei natürliche **Dekompositionen** — im Bericht begründen, welche gewählt und kombiniert werden:

### 4.1 Datenparallel: Permutations-Validierung

- **Idee:** Jeder Thread prüft einen Teil der `n!` Eingabepermutationen.
- **Wo:** `validator.cpp` — war schon mal drin, als Baseline entfernt.
- **Speedup:** Erst ab größerem `n` sinnvoll (n=3 zu klein; n=7+ relevant).
- **Messung:** 1 Thread vs T Threads, gleiche Netzwerk-Kandidaten.

### 4.2 Taskparallel: Kandidaten-Suche

- **Idee:** Kandidaten-Netzwerke / Teilbäume auf Threads verteilen.
- **Wo:** `search.cpp` — `filterValidNetworks`, evolutionary population.
- **Synchronisation:** Mutex oder Thread-Pool mit lokaler Sammlung + Merge.

### 4.3 Parallel in der Evolution (Haupthebel für großes n)

Nach **Sekanina (2004)** / **SorterHunter**:

- Population von Netzwerken, Fitness = swaps oder stages (bei Gültigkeit).
- Mutation / Crossover auf Stufen.
- **Parallel:** Population pro Generation parallel validieren und bewerten.
- Skaliert auf HPC: viele Individuen × teure Validierung.

| Phase | Parallelisierung | Erwarteter Speedup |
|-------|------------------|-------------------|
| v1 (jetzt) | Keine | 1 |
| v2 | Permutationen | moderat |
| v3 | + Kandidaten / Population | gut |
| v4 | OpenMP auf HPC | Ziel für Note > 2,0 |

**Technologie:** `std::thread` lokal → **OpenMP** (`#pragma omp parallel for`) auf HPC-Cluster.

---

## 5. Suchalgorithmus-Roadmap

### Stufe A — Serial Baseline (erledigt)

- Vollständige Enumeration für kleines `n`.
- Liefert **Referenzlösungen** (Goldstandard zum Testen).

### Stufe B — Parallel exhaustive (n klein, z. B. 3–7)

- Parallel validation + parallel candidate filtering.
- Vergleich Laufzeit 1 vs p Threads.
- Im Bericht: Speedup-Kurve für Validierung.

### Stufe C — Evolutionäre Suche (n mittel/groß)

Inspiriert von Sekanina + SorterHunter:

1. **Repräsentation:** Netzwerk = Liste von Stufen (disjunkte CS pro Stufe).
2. **Initialisierung:** Zufällig / aus bekannten kleinen Optima / Seeding.
3. **Fitness:**  
   - ungültig → sehr schlecht  
   - gültig → Anzahl Swaps oder Stufen (je nach `goal`)
4. **Operatoren:** Stufe hinzufügen/entfernen, CS tauschen, Stufen permutieren.
5. **Abbruch:** Keine Verbesserung über G Generationen / Zeitlimit.
6. **Ausgabe:** Bestes gefundenes Netzwerk (ggf. nicht bewiesen global optimal — im Bericht ehrlich diskutieren).

### Stufe D — Branch-and-Bound (optional, für swaps-Ziel)

- Untere Schranken aus Parberry-Tiefe / bekannte Median-Netzwerk-Grenzen.
- Nur wenn Zeit — evolutionary reicht meist für die Ausarbeitung.

---

## 6. Implementierungs-Phasen

### Phase 1 — Parallelisierung Baseline (2–3 Wochen)

| Task | Dateien | Ergebnis |
|------|---------|----------|
| Permutationen parallel | `validator.cpp` | `isValidMedianNetwork(..., threads)` |
| Kandidaten parallel | `search.cpp` | Thread-Pool in `filterValidNetworks` |
| Config `threads = N` | `config.cpp` | Steuerung ohne Codeänderung |
| Benchmarks parallel | `run_tests.cpp` | 1 vs N Threads in CSV |
| Doku | `performance-report.md` | Jede Änderung eintragen |

**Akzeptanz:** `make bench` zeigt Speedup > 1 für n=5 Validierung/Suche.

### Phase 2 — Evolutionäre Suche (3–4 Wochen)

| Task | Dateien | Ergebnis |
|------|---------|----------|
| `EvolutionConfig` | `include/evolution.hpp` | Population, Generationen, Mutationsrate |
| `evolution.cpp` | neuer Suchpfad | Ersetzt exhaustive ab `n >= 7` |
| Strategie-Wahl in config | `search_strategy = exhaustive \| evolutionary` | Automatische Wahl nach n |
| Tests mit bekannten Optima | `tests/fixtures/` | n=3,5 Regression |

**Akzeptanz:** n=7 in Minuten statt Stunden; korrekte Median-Eigenschaft.

### Phase 3 — HPC & Messung (2 Wochen, für Note > 2,0)

| Task | Ergebnis |
|------|----------|
| OpenMP-Version (`Makefile` Flag `OPENMP=1`) | Cluster-tauglich |
| Slurm-Job-Skript | Reproduzierbare Läufe |
| Mehrfachmessung | Mittelwert ± Schwankung |
| **Speedup** S(p) = T₁ / Tₚ | Tabelle + Grafik |
| **Effizienz** E(p) = S(p) / p | Kritische Diskussion |
| Problemgröße | Laufzeit **mehrere Minuten** auf p Kernen |

**Akzeptanz:** Berichtskapitel mit S und E, ehrliche Grenzen (Amdahl bei Validierung).

### Phase 4 — Bericht & Referat (laufend, finalisieren bis 31.8.)

---

## 7. Konfiguration (geplant)

```ini
# config.txt — Zielbild
n = 9
goal = stages
output = build/median_network.txt
threads = 8
search = evolutionary    # exhaustive | evolutionary | auto
population = 200
generations = 5000
max_stages = 20
max_swaps = 40
```

---

## 8. Test- & Benchmark-Plan

| Test | Zweck |
|------|-------|
| `make test` | Korrektheit klein (n=3), schnell |
| `make bench` | n=5 exhaustive + Timing |
| `make test-parallel` | Speedup-Tests (neu) |
| `make test-evolution` | Evolution vs exhaustive auf n=5 (gleiches Optimum) |
| HPC batch | großes n, CSV nach `results/` |

**Korrektheitskriterium:** Für jedes gefundene Netzwerk: alle Permutationen → Median auf Kanal `n/2`.

---

## 9. Projektbericht (~8 Seiten / ~12 000 Zeichen)

1. **Einleitung** — Median-Netzwerk, Abgrenzung Sortiernetzwerk  
2. **Problemdefinition** — Formal: Stufen, CS, Zielfunktionen (i)/(ii)  
3. **Dekomposition** — Warum Permutationen / Kandidaten / Population parallel  
4. **Implementierung** — Architektur, Datenstrukturen, Pseudocode  
5. **Evolutionärer Ansatz** — Bezug Sekanina, Operatoren, Parameter  
6. **Messungen** — Release, Hardware, HPC, Tabellen S & E  
7. **Ergebnisse** — Gefundene Netzwerke für gewählte n  
8. **Diskussion** — Amdahl, Grenzen exhaustive, evolutionary nicht optimal?  
9. **Quellen** — Parberry, Sekanina, SorterHunter, Vorlesung  

**Referat (15 min):** Live-Demo `config.txt` → Netzwerk → Parallelisungsgewinn zeigen.

---

## 10. Risiken & Mitigation

| Risiko | Mitigation |
|--------|------------|
| Exhaustive zu langsam | Evolution ab n≥7; exhaustive nur Referenz |
| Parallel zu langsam bei kleinem n | Nur großes n für HPC-Messung |
| Evolution findet kein Optimum | Mit bekannten n=3,5 verifizieren; Parameter tunen |
| 39 000 optimale Netze n=5 | Nur **ein** repräsentatives ausgeben oder kanonische Form |
| Zeit bis 31.8. | Phase 1+2 zuerst; HPC früh testen (Zugang!) |

---

## 11. Nächste konkrete Schritte

1. **Phase 1 starten:** Parallel permutation validation in `validator.cpp`  
2. **`threads` zurück in `config.txt`** und `SearchConfig`  
3. **Benchmark 1 vs N Threads** für n=5 in `make bench`  
4. **`performance-report.md`** aktualisieren nach jeder Optimierung  
5. **Evolution-Prototyp** skizzieren (`evolution.hpp`) — Population + Fitness  
6. **HPC-Zugang** klären (Osnabrück Cluster) — Account, Slurm, Compiler  

---

## 12. Bezug zu Quellen

| Quelle | Nutzen im Projekt |
|--------|-------------------|
| **Parberry (1991)** | Untere Schranken Tiefe; Kontext Sortiernetze |
| **Sekanina (2004)** | Evolutionäre Median-Schaltungen — Hauptreferenz für Suche |
| **SorterHunter** | Inspirationscode, Darstellung, ggf. Vergleichswerte |
| **Vorlesung PVA** | Dekomposition, Speedup, Effizienz, OpenMP |
