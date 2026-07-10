// median_search.cpp
// Suche nach groessen- bzw. tiefenoptimalen Median-Netzwerken (n ungerade).
//
// Konventionen:
//   * Kanaele 0..n-1. Komparator [x,y] mit x<y: Minimum -> x, Maximum -> y.
//   * Der Median muss nach dem Netzwerk auf Kanal m = (n-1)/2 liegen.
//
//   * Parallelisierung: Intel oneTBB (parallel_for_each, concurrent_vector).
//     Praefixtiefe 3 im Groessenmodus fuer feinere Tasks / Work-Stealing.
//
// Build:   make  (requires Intel oneTBB)
// Beispiel: ./build/p_suche --n 7 --opt depth --compare --threads 8 --out netz.txt

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include <tbb/concurrent_vector.h>
#include <tbb/global_control.h>
#include <tbb/parallel_for_each.h>
#include <tbb/task_arena.h>

using u32 = uint32_t;
using u64 = uint64_t;

struct Cmp {
    uint8_t i, j;
};
using Stage = std::vector<Cmp>;
using Net = std::vector<Stage>;

static int N = 0;
static int M = 0;
static int B = 0;
static int T = 0;
static std::vector<u64> INIT;
static std::vector<u64> TARGET;
static std::vector<Cmp> PAIRS;
static std::vector<int> IDX_ALL;
static std::vector<int> IDX_M;

static bool F_CANON = true;
static bool F_SUPP = true;
static bool F_LASTM = true;
static bool F_REDUN = true;
static bool F_MIRROR = true;
static bool F_FIRST = false;
static bool F_VERIFY = true;
static long F_LIMIT = 0;
static int F_LB = -1;
static int F_THREADS = 0;
static bool F_PARALLEL = true;
static bool F_COMPARE = false;

static std::atomic<bool> g_stop{false};
static std::atomic<unsigned long long> g_nodes{0};
static tbb::concurrent_vector<Net> g_sols;
static std::unique_ptr<tbb::global_control> g_thread_control;

static int effectiveThreads() {
    if (F_THREADS > 0) {
        return F_THREADS;
    }
    return tbb::this_task_arena::max_concurrency();
}

static void setup_thread_control() {
    g_thread_control.reset();
    if (F_PARALLEL && F_THREADS > 0) {
        g_thread_control = std::make_unique<tbb::global_control>(
            tbb::global_control::max_allowed_parallelism, static_cast<std::size_t>(F_THREADS));
    }
}

template <typename Container, typename Fn>
static void foreach_task(const Container& tasks, Fn fn) {
    if (!F_PARALLEL || tasks.size() <= 1) {
        for (const auto& task : tasks) {
            fn(task);
        }
        return;
    }
    tbb::parallel_for_each(tasks.begin(), tasks.end(), fn);
}

static int pc32(u32 x) { return __builtin_popcount(x); }

static unsigned long long telephone(int n) {
    std::vector<unsigned long long> t(static_cast<std::size_t>(n) + 1);
    t[0] = 1;
    if (n >= 1) {
        t[1] = 1;
    }
    for (int k = 2; k <= n; k++) {
        t[k] = t[k - 1] + static_cast<unsigned long long>(k - 1) * t[k - 2];
    }
    return t[n];
}

static void build_problem() {
    M = (N - 1) / 2;
    std::vector<u32> vecs;
    for (int k : {(N - 1) / 2, (N + 1) / 2}) {
        u32 v = (k == 0) ? 0u : ((1u << k) - 1u);
        if (k == 0) {
            vecs.push_back(0u);
            continue;
        }
        while (v < (1u << N)) {
            vecs.push_back(v);
            const u32 c = v & (~v + 1u);
            const u32 r = v + c;
            v = r | (((v ^ r) >> 2) / c);
        }
    }
    T = static_cast<int>(vecs.size());
    B = (T + 63) / 64;
    INIT.assign(static_cast<std::size_t>(N) * B, 0);
    TARGET.assign(B, 0);
    const int hi = (N + 1) / 2;
    for (int t = 0; t < T; t++) {
        const u32 v = vecs[static_cast<std::size_t>(t)];
        for (int i = 0; i < N; i++) {
            if ((v >> i) & 1u) {
                INIT[static_cast<std::size_t>(i) * B + t / 64] |= 1ull << (t % 64);
            }
        }
        if (pc32(v) == hi) {
            TARGET[t / 64] |= 1ull << (t % 64);
        }
    }
    PAIRS.clear();
    IDX_ALL.clear();
    IDX_M.clear();
    for (int i = 0; i < N; i++) {
        for (int j = i + 1; j < N; j++) {
            if (i == M || j == M) {
                IDX_M.push_back(static_cast<int>(PAIRS.size()));
            }
            IDX_ALL.push_back(static_cast<int>(PAIRS.size()));
            PAIRS.push_back({static_cast<uint8_t>(i), static_cast<uint8_t>(j)});
        }
    }
}

struct Engine {
    std::vector<u64> R;
    std::vector<u32> S;
    std::vector<u64> undoRows;
    struct U {
        int i, j;
        u32 si, sj;
    };
    std::vector<U> undoMeta;
    std::vector<Cmp> seq;
    unsigned long long nodes = 0;

    Engine() : R(INIT), S(N) {
        for (int i = 0; i < N; i++) {
            S[i] = 1u << i;
        }
    }

    bool useful(int i, int j) const {
        const u64* a = &R[static_cast<std::size_t>(i) * B];
        const u64* b = &R[static_cast<std::size_t>(j) * B];
        for (int t = 0; t < B; t++) {
            if (a[t] & ~b[t]) {
                return true;
            }
        }
        return false;
    }

    void apply(int i, int j) {
        undoMeta.push_back({i, j, S[i], S[j]});
        const size_t off = undoRows.size();
        undoRows.resize(off + 2 * static_cast<std::size_t>(B));
        u64* a = &R[static_cast<std::size_t>(i) * B];
        u64* b = &R[static_cast<std::size_t>(j) * B];
        std::memcpy(&undoRows[off], a, 8u * B);
        std::memcpy(&undoRows[off + B], b, 8u * B);
        for (int t = 0; t < B; t++) {
            const u64 x = a[t];
            const u64 y = b[t];
            a[t] = x & y;
            b[t] = x | y;
        }
        const u32 s = S[i] | S[j];
        S[i] = s;
        S[j] = s;
    }

    void undo() {
        const U u = undoMeta.back();
        undoMeta.pop_back();
        const size_t off = undoRows.size() - 2 * static_cast<std::size_t>(B);
        std::memcpy(&R[static_cast<std::size_t>(u.i) * B], &undoRows[off], 8u * B);
        std::memcpy(&R[static_cast<std::size_t>(u.j) * B], &undoRows[off + B], 8u * B);
        undoRows.resize(off);
        S[u.i] = u.si;
        S[u.j] = u.sj;
    }

    bool done() const {
        const u64* mr = &R[static_cast<std::size_t>(M) * B];
        for (int t = 0; t < B; t++) {
            if (mr[t] != TARGET[t]) {
                return false;
            }
        }
        return true;
    }

    bool suppOK(int others) const {
        if (others >= N - 1) {
            return true;
        }
        const int need = N - pc32(S[M]);
        if (need <= 0) {
            return true;
        }
        int cnt[32];
        int k = 0;
        const u32 sm = S[M];
        for (int i = 0; i < N; i++) {
            if (i != M) {
                cnt[k++] = pc32(S[i] & ~sm);
            }
        }
        int sum = 0;
        for (int t = 0; t < others; t++) {
            int bi = -1;
            int bv = 0;
            for (int q = 0; q < k; q++) {
                if (cnt[q] > bv) {
                    bv = cnt[q];
                    bi = q;
                }
            }
            if (bi < 0) {
                break;
            }
            sum += bv;
            cnt[bi] = 0;
            if (sum >= need) {
                return true;
            }
        }
        return sum >= need;
    }
};

static Net levelize(const std::vector<Cmp>& seq) {
    std::vector<int> lvl(N, 0);
    Net net;
    for (Cmp c : seq) {
        const int l = std::max(lvl[c.i], lvl[c.j]) + 1;
        if (static_cast<int>(net.size()) < l) {
            net.resize(l);
        }
        net[static_cast<std::size_t>(l - 1)].push_back(c);
        lvl[c.i] = lvl[c.j] = l;
    }
    return net;
}

static void sort_stages(Net& n) {
    for (auto& st : n) {
        std::sort(st.begin(), st.end(), [](Cmp a, Cmp b) {
            return a.i < b.i || (a.i == b.i && a.j < b.j);
        });
    }
}

static std::string serialize(const Net& n0) {
    Net n = n0;
    sort_stages(n);
    std::string s;
    for (std::size_t k = 0; k < n.size(); k++) {
        if (k) {
            s += '\n';
        }
        for (std::size_t q = 0; q < n[k].size(); q++) {
            if (q) {
                s += ',';
            }
            s += '[' + std::to_string(static_cast<int>(n[k][q].i)) + ',' +
                 std::to_string(static_cast<int>(n[k][q].j)) + ']';
        }
    }
    return s;
}

static Net reflect(const Net& n) {
    Net r = n;
    for (auto& st : r) {
        for (auto& c : st) {
            const uint8_t i = static_cast<uint8_t>(N - 1 - c.j);
            const uint8_t j = static_cast<uint8_t>(N - 1 - c.i);
            c = {i, j};
        }
    }
    sort_stages(r);
    return r;
}

static bool net_ok_threshold(const Net& n) {
    std::vector<u64> R = INIT;
    for (const auto& st : n) {
        for (Cmp c : st) {
            u64* a = &R[static_cast<std::size_t>(c.i) * B];
            u64* b = &R[static_cast<std::size_t>(c.j) * B];
            for (int t = 0; t < B; t++) {
                const u64 x = a[t];
                const u64 y = b[t];
                a[t] = x & y;
                b[t] = x | y;
            }
        }
    }
    const u64* mr = &R[static_cast<std::size_t>(M) * B];
    for (int t = 0; t < B; t++) {
        if (mr[t] != TARGET[t]) {
            return false;
        }
    }
    return true;
}

static bool verify_full(const Net& n) {
    for (u32 x = 0; x < (1u << N); x++) {
        u32 v = x;
        for (const auto& st : n) {
            for (Cmp c : st) {
                const u32 bi = (v >> c.i) & 1u;
                const u32 bj = (v >> c.j) & 1u;
                const u32 lo = bi & bj;
                const u32 hi = bi | bj;
                v = (v & ~((1u << c.i) | (1u << c.j))) | (lo << c.i) | (hi << c.j);
            }
        }
        const u32 maj = static_cast<u32>(pc32(x) > N / 2);
        if (((v >> M) & 1u) != maj) {
            return false;
        }
    }
    return true;
}

static bool reducible(const Net& n) {
    int total = 0;
    for (const auto& st : n) {
        total += static_cast<int>(st.size());
    }
    for (int skip = 0; skip < total; skip++) {
        Net m;
        int idx = 0;
        for (const auto& st : n) {
            Stage s2;
            for (Cmp c : st) {
                if (idx != skip) {
                    s2.push_back(c);
                }
                idx++;
            }
            if (!s2.empty()) {
                m.push_back(s2);
            }
        }
        if (net_ok_threshold(m)) {
            return true;
        }
    }
    return false;
}

static void record(const Net& net) {
    if (g_stop.load(std::memory_order_relaxed)) {
        return;
    }
    g_sols.push_back(net);
    if (F_FIRST || (F_LIMIT > 0 && static_cast<long>(g_sols.size()) >= F_LIMIT)) {
        g_stop.store(true);
    }
}

static void dfs_size(Engine& E, int rem, int prevIdx) {
    if (g_stop.load(std::memory_order_relaxed)) {
        return;
    }
    E.nodes++;
    if (rem == 0) {
        if (E.done()) {
            record(levelize(E.seq));
        }
        return;
    }
    if (F_SUPP && !E.suppOK(rem)) {
        return;
    }
    const std::vector<int>& cand = (F_LASTM && rem == 1) ? IDX_M : IDX_ALL;
    for (int idx : cand) {
        const Cmp c = PAIRS[static_cast<std::size_t>(idx)];
        if (F_CANON && prevIdx >= 0 && idx < prevIdx) {
            const Cmp p = PAIRS[static_cast<std::size_t>(prevIdx)];
            const bool share = (c.i == p.i || c.i == p.j || c.j == p.i || c.j == p.j);
            if (!share) {
                continue;
            }
        }
        if (F_REDUN && !E.useful(c.i, c.j)) {
            continue;
        }
        E.apply(c.i, c.j);
        E.seq.push_back(c);
        dfs_size(E, rem - 1, idx);
        E.seq.pop_back();
        E.undo();
    }
}

struct STask {
    std::vector<int> idxs;
};

static void gen_prefix(Engine& E, int budget, int P, int prevIdx, std::vector<int>& cur,
                       std::vector<STask>& out) {
    if (static_cast<int>(cur.size()) == P) {
        out.push_back({cur});
        return;
    }
    const int rem = budget - static_cast<int>(cur.size());
    if (F_SUPP && !E.suppOK(rem)) {
        return;
    }
    const std::vector<int>& cand = (F_LASTM && rem == 1) ? IDX_M : IDX_ALL;
    for (int idx : cand) {
        const Cmp c = PAIRS[static_cast<std::size_t>(idx)];
        if (F_CANON && prevIdx >= 0 && idx < prevIdx) {
            const Cmp p = PAIRS[static_cast<std::size_t>(prevIdx)];
            const bool share = (c.i == p.i || c.i == p.j || c.j == p.i || c.j == p.j);
            if (!share) {
                continue;
            }
        }
        if (F_REDUN && !E.useful(c.i, c.j)) {
            continue;
        }
        E.apply(c.i, c.j);
        cur.push_back(idx);
        gen_prefix(E, budget, P, idx, cur, out);
        cur.pop_back();
        E.undo();
    }
}

static int run_size() {
    const int lb = (F_LB >= 0) ? F_LB : 1;
    for (int s = lb;; s++) {
        g_stop.store(false);
        g_sols.clear();
        const unsigned long long n0 = g_nodes.load();
        const auto t0 = std::chrono::steady_clock::now();

        std::vector<STask> tasks;
        const int P = std::min(3, s);
        {
            Engine E;
            std::vector<int> cur;
            gen_prefix(E, s, P, -1, cur, tasks);
        }

        foreach_task(tasks, [&](const STask& tk) {
            if (g_stop.load(std::memory_order_relaxed)) {
                return;
            }
            Engine E;
            int prev = -1;
            for (int idx : tk.idxs) {
                const Cmp c = PAIRS[static_cast<std::size_t>(idx)];
                E.apply(c.i, c.j);
                E.seq.push_back(c);
                prev = idx;
            }
            dfs_size(E, s - P, prev);
            g_nodes.fetch_add(E.nodes, std::memory_order_relaxed);
        });

        const double dt =
            std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
        std::printf("Budget %2d: Tasks=%zu  Knoten=%llu  Zeit=%.2fs  Loesungen=%zu\n", s,
                    tasks.size(), g_nodes.load() - n0, dt, g_sols.size());
        std::fflush(stdout);
        if (!g_sols.empty()) {
            return s;
        }
    }
}

static void dfs_depth(Engine& E, Net& net, int rem);

static void last_stage(Engine& E, Net& net) {
    const u64* Rm = &E.R[static_cast<std::size_t>(M) * B];
    for (int j = 0; j < N; j++) {
        if (j == M) {
            continue;
        }
        const u64* Rj = &E.R[static_cast<std::size_t>(j) * B];
        bool ok = true;
        if (j < M) {
            for (int t = 0; t < B; t++) {
                if ((Rm[t] | Rj[t]) != TARGET[t]) {
                    ok = false;
                    break;
                }
            }
        } else {
            for (int t = 0; t < B; t++) {
                if ((Rm[t] & Rj[t]) != TARGET[t]) {
                    ok = false;
                    break;
                }
            }
        }
        if (ok) {
            net.push_back({{static_cast<uint8_t>(std::min(j, M)), static_cast<uint8_t>(std::max(j, M))}});
            record(net);
            net.pop_back();
        }
    }
}

static void build_stage(Engine& E, Net& net, int rem, int ch, u32 used, int cnt) {
    if (g_stop.load(std::memory_order_relaxed)) {
        return;
    }
    while (ch < N && ((used >> ch) & 1u)) {
        ch++;
    }
    if (ch >= N) {
        if (cnt > 0) {
            dfs_depth(E, net, rem - 1);
        }
        return;
    }
    build_stage(E, net, rem, ch + 1, used | (1u << ch), cnt);
    for (int j = ch + 1; j < N; j++) {
        if ((used >> j) & 1u) {
            continue;
        }
        if (F_REDUN && !E.useful(ch, j)) {
            continue;
        }
        E.apply(ch, j);
        net.back().push_back({static_cast<uint8_t>(ch), static_cast<uint8_t>(j)});
        build_stage(E, net, rem, ch + 1, used | (1u << ch) | (1u << j), cnt + 1);
        net.back().pop_back();
        E.undo();
    }
}

static void dfs_depth(Engine& E, Net& net, int rem) {
    if (g_stop.load(std::memory_order_relaxed)) {
        return;
    }
    E.nodes++;
    if (rem == 1) {
        last_stage(E, net);
        return;
    }
    if (F_SUPP) {
        const int allow = (rem >= 20) ? N : (1 << rem) - 1;
        if (!E.suppOK(std::min(allow, N))) {
            return;
        }
    }
    net.push_back({});
    build_stage(E, net, rem, 0, 0u, 0);
    net.pop_back();
}

static void gen_stage1(int ch, u32 used, Stage& cur, std::vector<Stage>& out) {
    while (ch < N && ((used >> ch) & 1u)) {
        ch++;
    }
    if (ch >= N) {
        if (!cur.empty()) {
            out.push_back(cur);
        }
        return;
    }
    gen_stage1(ch + 1, used | (1u << ch), cur, out);
    for (int j = ch + 1; j < N; j++) {
        if ((used >> j) & 1u) {
            continue;
        }
        cur.push_back({static_cast<uint8_t>(ch), static_cast<uint8_t>(j)});
        gen_stage1(ch + 1, used | (1u << ch) | (1u << j), cur, out);
        cur.pop_back();
    }
}

static int run_depth() {
    int lb0 = 0;
    while ((1 << lb0) < N) {
        lb0++;
    }
    const int lb = (F_LB >= 0) ? F_LB : lb0;
    for (int d = lb;; d++) {
        g_stop.store(false);
        g_sols.clear();
        const unsigned long long n0 = g_nodes.load();
        const auto t0 = std::chrono::steady_clock::now();
        std::size_t ntasks = 0;

        if (d == 1) {
            Engine E;
            Net net;
            last_stage(E, net);
        } else if (d > 1) {
            std::vector<Stage> tasks;
            Stage cur;
            gen_stage1(0, 0u, cur, tasks);
            if (F_MIRROR) {
                std::vector<Stage> keep;
                for (auto& st : tasks) {
                    const Net a{st};
                    if (serialize(a) <= serialize(reflect(a))) {
                        keep.push_back(st);
                    }
                }
                tasks.swap(keep);
            }
            ntasks = tasks.size();
            foreach_task(tasks, [&](const Stage& st) {
                if (g_stop.load(std::memory_order_relaxed)) {
                    return;
                }
                Engine E;
                Net net;
                net.push_back(st);
                for (Cmp c : st) {
                    E.apply(c.i, c.j);
                }
                dfs_depth(E, net, d - 1);
                g_nodes.fetch_add(E.nodes, std::memory_order_relaxed);
            });
        }

        const double dt =
            std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
        std::printf("Stufen %2d: Tasks=%zu  Knoten=%llu  Zeit=%.2fs  Loesungen=%zu\n", d, ntasks,
                    g_nodes.load() - n0, dt, g_sols.size());
        std::fflush(stdout);
        if (!g_sols.empty()) {
            return d;
        }
    }
}

static void print_usage() {
    std::fprintf(stderr,
                 "Usage: p_suche --n <odd> --opt size|depth [--threads N] [--out file]\n"
                 "       [--sequential] [--compare] [--first] [--limit k] [--lb b]\n"
                 "       [--no-verify] [--no-canon] [--no-supp] [--no-lastm]\n"
                 "       [--no-redun] [--no-mirror]\n");
}

static int run_search(const std::string& mode) {
    return (mode == "depth") ? run_depth() : run_size();
}

static void reset_search_state() {
    g_stop.store(false);
    g_sols.clear();
    g_nodes.store(0);
}

static bool write_solutions(const std::string& outfile, const std::string& mode) {
    std::vector<Net> sols(g_sols.begin(), g_sols.end());
    if (mode == "depth") {
        std::vector<Net> keep;
        for (auto& n : sols) {
            if (!reducible(n)) {
                keep.push_back(n);
            }
        }
        const std::size_t base = keep.size();
        for (std::size_t k = 0; k < base; k++) {
            keep.push_back(reflect(keep[k]));
        }
        sols.swap(keep);
    }

    std::vector<std::pair<std::string, Net>> ser;
    for (auto& n : sols) {
        ser.push_back({serialize(n), n});
    }
    std::sort(ser.begin(), ser.end(),
              [](const auto& a, const auto& b) { return a.first < b.first; });
    ser.erase(std::unique(ser.begin(), ser.end(),
                          [](const auto& a, const auto& b) { return a.first == b.first; }),
              ser.end());
    if (F_LIMIT > 0 && static_cast<long>(ser.size()) > F_LIMIT) {
        ser.resize(static_cast<std::size_t>(F_LIMIT));
    }

    FILE* f = std::fopen(outfile.c_str(), "w");
    if (!f) {
        std::fprintf(stderr, "Kann %s nicht schreiben.\n", outfile.c_str());
        return false;
    }
    for (std::size_t k = 0; k < ser.size(); k++) {
        if (k) {
            std::fputc('\n', f);
        }
        std::fputs(ser[k].first.c_str(), f);
        std::fputc('\n', f);
    }
    std::fclose(f);

    bool allok = true;
    if (F_VERIFY) {
        for (auto& p : ser) {
            if (!verify_full(p.second)) {
                allok = false;
                break;
            }
        }
    }
    return allok;
}

int main(int argc, char** argv) {
    std::string mode = "size";
    std::string outfile;

    for (int a = 1; a < argc; a++) {
        const std::string s = argv[a];
        auto next = [&]() -> const char* { return (a + 1 < argc) ? argv[++a] : ""; };
        if (s == "--n") {
            N = std::atoi(next());
        } else if (s == "--opt") {
            mode = next();
        } else if (s == "--threads") {
            F_THREADS = std::atoi(next());
        } else if (s == "--sequential") {
            F_PARALLEL = false;
        } else if (s == "--compare") {
            F_COMPARE = true;
        } else if (s == "--out") {
            outfile = next();
        } else if (s == "--limit") {
            F_LIMIT = std::atol(next());
        } else if (s == "--lb") {
            F_LB = std::atoi(next());
        } else if (s == "--first") {
            F_FIRST = true;
        } else if (s == "--no-verify") {
            F_VERIFY = false;
        } else if (s == "--no-canon") {
            F_CANON = false;
        } else if (s == "--no-supp") {
            F_SUPP = false;
        } else if (s == "--no-lastm") {
            F_LASTM = false;
        } else if (s == "--no-redun") {
            F_REDUN = false;
        } else if (s == "--no-mirror") {
            F_MIRROR = false;
        } else if (s == "--help" || s == "-h") {
            print_usage();
            return 0;
        } else {
            std::fprintf(stderr, "Unbekanntes Argument: %s\n", s.c_str());
            print_usage();
            return 2;
        }
    }

    if (N < 1 || N > 19 || N % 2 == 0) {
        std::fprintf(stderr, "Bitte --n ungerade, 1..19 angeben.\n");
        return 2;
    }
    if (mode != "size" && mode != "depth") {
        std::fprintf(stderr, "Bitte --opt size oder depth angeben.\n");
        return 2;
    }
    if (outfile.empty()) {
        outfile = "build/median_n" + std::to_string(N) + "_" + mode + ".txt";
    }

    const int th = effectiveThreads();
    const char* parLabel = F_PARALLEL ? "tbb" : "sequential";

    if (N == 1) {
        FILE* f = std::fopen(outfile.c_str(), "w");
        if (f) {
            std::fclose(f);
        }
        std::printf("n=1: leeres Netzwerk ist optimal (0 Komparatoren, 0 Stufen).\n");
        return 0;
    }

    build_problem();
    std::printf("n=%d  Zielkanal m=%d  Testvektoren=%d (statt 2^n=%u)  Paare=%zu  "
                "Matchings=%llu  Parallel=%s  Threads=%d  Modus=%s\n",
                N, M, T, 1u << N, PAIRS.size(), telephone(N) - 1, parLabel, th, mode.c_str());
    if (F_COMPARE) {
        std::printf("Vergleichsmodus: sequentiell, dann TBB-parallel\n");
    }
    if (F_LB >= 0) {
        std::printf("Hinweis: --lb gesetzt; Optimalitaet gilt nur unter der Annahme, "
                    "dass kleinere Budgets unloesbar sind.\n");
    }
    std::fflush(stdout);

    int opt = 0;
    double dt = 0.0;
    double seqTime = 0.0;
    double parTime = 0.0;
    int seqOpt = 0;
    int parOpt = 0;

    if (F_COMPARE) {
        reset_search_state();
        F_PARALLEL = false;
        setup_thread_control();
        const auto t0 = std::chrono::steady_clock::now();
        seqOpt = run_search(mode);
        seqTime = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
        const unsigned long long seqNodes = g_nodes.load();

        reset_search_state();
        F_PARALLEL = true;
        setup_thread_control();
        const auto t1 = std::chrono::steady_clock::now();
        parOpt = run_search(mode);
        parTime = std::chrono::duration<double>(std::chrono::steady_clock::now() - t1).count();
        const unsigned long long parNodes = g_nodes.load();

        opt = parOpt;
        dt = parTime;

        const double speedup = parTime > 0.0 ? seqTime / parTime : 0.0;
        const double efficiency = (th > 0 && speedup > 0.0) ? (100.0 * speedup / th) : 0.0;
        std::printf("\n=== Vergleich sequentiell vs TBB ===\n");
        std::printf("Sequentiell:  %.4fs  Optimal=%d  Knoten=%llu\n", seqTime, seqOpt, seqNodes);
        std::printf("Parallel:     %.4fs  Optimal=%d  Knoten=%llu  Threads=%d\n", parTime, parOpt,
                    parNodes, th);
        std::printf("Speedup S:    %.2f\n", speedup);
        std::printf("Effizienz E:  %.1f%%\n", efficiency);
        if (seqOpt != parOpt) {
            std::printf("Warnung: Optimalwerte unterscheiden sich (%d vs %d)\n", seqOpt, parOpt);
        }
    } else {
        setup_thread_control();
        const auto t0 = std::chrono::steady_clock::now();
        opt = run_search(mode);
        dt = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    }

    const bool allok = write_solutions(outfile, mode);

    std::printf("Optimal (%s): %d   Netzwerke: %zu   Gesamtknoten=%llu   Gesamtzeit=%.2fs\n",
                mode.c_str(), opt, g_sols.size(), g_nodes.load(), dt);
    if (F_VERIFY) {
        std::printf("Brute-Force-Verifikation ueber alle %u Eingaben: %s\n", 1u << N,
                    allok ? "OK" : "FEHLER");
    }
    if (F_FIRST || (F_LIMIT > 0)) {
        std::printf("Hinweis: Aufzaehlung ggf. unvollstaendig (--first/--limit); "
                    "der Optimalwert selbst bleibt gueltig.\n");
    }
    std::printf("Ausgabedatei: %s\n", outfile.c_str());
    return allok ? 0 : 1;
}
