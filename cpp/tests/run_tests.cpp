#include "test_report.hpp"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

namespace {

constexpr const char* kBinary = "build/p_suche";
constexpr const char* kTestResultsCsv = "results/test_results.csv";
constexpr const char* kBenchmarkResultsCsv = "results/benchmark_results.csv";

int failures = 0;
TestReport report;

using Clock = std::chrono::steady_clock;

double elapsedMs(const Clock::time_point& start, const Clock::time_point& end) {
    return std::chrono::duration<double, std::milli>(end - start).count();
}

void check(bool condition, const char* expression, const char* file, int line) {
    if (!condition) {
        std::cerr << "FAIL " << file << ':' << line << "  " << expression << '\n';
        ++failures;
    }
}

#define CHECK(cond) check((cond), #cond, __FILE__, __LINE__)

const char* buildModeLabel() {
#ifdef NDEBUG
    return "release";
#else
    return "debug";
#endif
}

int runCommand(const std::string& cmd, std::string& output) {
    output.clear();
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) {
        return -1;
    }
    char buffer[512];
    while (std::fgets(buffer, sizeof(buffer), pipe)) {
        output += buffer;
    }
    const int status = pclose(pipe);
    return status;
}

bool readFile(const std::string& path, std::string& content) {
    std::ifstream file(path);
    if (!file) {
        return false;
    }
    std::ostringstream out;
    out << file.rdbuf();
    content = out.str();
    return true;
}

template <typename Fn>
void runTest(const char* name, Fn&& fn) {
    const int failuresBefore = failures;
    const Clock::time_point start = Clock::now();
    fn();
    TestRecord record;
    record.name = name;
    record.status = failures == failuresBefore ? "pass" : "fail";
    record.durationMs = elapsedMs(start, Clock::now());
    report.addTest(record);
}

void testHelp() {
    std::string output;
    const int status = runCommand("./" + std::string(kBinary) + " --help 2>&1", output);
    CHECK(status == 0);
    CHECK(output.find("--n") != std::string::npos);
    CHECK(output.find("--opt") != std::string::npos);
}

void testN3SizeOptimum() {
    const std::string out = "build/test_n3_size.txt";
    std::string cmd = "./" + std::string(kBinary) +
                      " --n 3 --opt size --sequential --first --out " + out + " 2>&1";
    std::string output;
    const int status = runCommand(cmd, output);
    CHECK(status == 0);
    CHECK(output.find("Optimal (size): 3") != std::string::npos);

    std::string content;
    CHECK(readFile(out, content));
    CHECK(content.find("[0,1]") != std::string::npos);
    CHECK(content.find("[0,2]") != std::string::npos);
    CHECK(content.find("[1,2]") != std::string::npos);
}

void testN3DepthOptimum() {
    const std::string out = "build/test_n3_depth.txt";
    std::string cmd = "./" + std::string(kBinary) +
                      " --n 3 --opt depth --sequential --first --out " + out + " 2>&1";
    std::string output;
    const int status = runCommand(cmd, output);
    CHECK(status == 0);
    CHECK(output.find("Optimal (depth): 3") != std::string::npos);
}

void testOutputFormatN5() {
    const std::string out = "build/test_n5_size_first.txt";
    std::string cmd = "./" + std::string(kBinary) +
                      " --n 5 --opt size --sequential --first --out " + out + " 2>&1";
    std::string output;
    const int status = runCommand(cmd, output);
    CHECK(status == 0);
    CHECK(output.find("Optimal (size): 7") != std::string::npos);

    std::string content;
    CHECK(readFile(out, content));
    CHECK(content.find('[') != std::string::npos);
    CHECK(content.find(',') != std::string::npos);
}

void testInvalidN() {
    std::string output;
    const int status = runCommand("./" + std::string(kBinary) + " --n 4 --opt size 2>&1", output);
    CHECK(status != 0);
}

void testCompareMode() {
    std::string cmd = "./" + std::string(kBinary) +
                      " --n 5 --opt size --compare --first --threads 4 --out build/test_compare.txt 2>&1";
    std::string output;
    const int status = runCommand(cmd, output);
    CHECK(status == 0);
    CHECK(output.find("=== Vergleich sequentiell vs TBB ===") != std::string::npos);
    CHECK(output.find("Speedup S:") != std::string::npos);
    CHECK(output.find("Effizienz E:") != std::string::npos);
    CHECK(output.find("Optimal (size): 7") != std::string::npos);
}

void runBenchmarks() {
    std::cout << "\nBenchmarks (build=" << buildModeLabel() << ", TBB):\n";

    auto bench = [&](const char* name, const std::string& cmd, const std::string& detail,
                     double speedup = -1.0) {
        const Clock::time_point start = Clock::now();
        std::string output;
        const int status = runCommand(cmd, output);
        const double ms = elapsedMs(start, Clock::now());
        BenchmarkRecord record;
        record.name = name;
        record.durationMs = ms;
        record.speedup = speedup;
        record.detail = detail + (status == 0 ? "" : " [exit!=" + std::to_string(status) + "]");
        report.addBenchmark(record);
        std::cout << "BENCH " << name << "  " << ms << " ms";
        if (speedup > 0.0) {
            std::cout << "  speedup=" << speedup;
        }
        std::cout << "  " << record.detail << '\n';
    };

    std::string compareCmd = "./" + std::string(kBinary) +
                             " --n 5 --opt size --compare --first --threads 0 "
                             "--out build/bench_compare_n5.txt 2>&1";
    std::string output;
    const Clock::time_point start = Clock::now();
    const int status = runCommand(compareCmd, output);
    const double ms = elapsedMs(start, Clock::now());

    double speedup = -1.0;
    const std::string speedupKey = "Speedup S:";
    const std::size_t pos = output.find(speedupKey);
    if (pos != std::string::npos) {
        speedup = std::atof(output.c_str() + pos + speedupKey.size());
    }

    BenchmarkRecord record;
    record.name = "compare n=5 size sequential vs tbb";
    record.durationMs = ms;
    record.speedup = speedup;
    record.detail = "one run: --compare";
    if (status != 0) {
        record.detail += " [exit!=" + std::to_string(status) + "]";
    }
    report.addBenchmark(record);
    std::cout << "BENCH " << record.name << "  " << ms << " ms";
    if (speedup > 0.0) {
        std::cout << "  speedup=" << speedup;
    }
    std::cout << "  " << record.detail << '\n';

    bench("search n=5 depth sequential",
          "./" + std::string(kBinary) +
              " --n 5 --opt depth --sequential --first --threads 1 --out build/bench_n5_depth.txt 2>&1",
          "proven optimum");
}

bool writeReportCsvFiles() {
    const std::string timestamp = currentTimestamp();
    const std::string build = buildModeLabel();
    bool ok = true;
    if (report.testCount() > 0) {
        ok = report.writeTestsCsv(kTestResultsCsv, timestamp, build, 1) && ok;
    }
    if (report.benchmarkCount() > 0) {
        ok = report.writeBenchmarksCsv(kBenchmarkResultsCsv, timestamp, build, 1) && ok;
    }
    return ok;
}

}  // namespace

int main() {
    if (std::getenv("P_SUCHE_BENCH")) {
        runBenchmarks();
        writeReportCsvFiles();
        return 0;
    }

    runTest("testHelp", testHelp);
    runTest("testInvalidN", testInvalidN);
    runTest("testN3SizeOptimum", testN3SizeOptimum);
    runTest("testN3DepthOptimum", testN3DepthOptimum);
    runTest("testOutputFormatN5", testOutputFormatN5);
    runTest("testCompareMode", testCompareMode);

    if (failures == 0) {
        std::cout << "All tests passed.\n";
        runBenchmarks();
    } else {
        std::cerr << failures << " test(s) failed.\n";
    }

    writeReportCsvFiles();
    return failures == 0 ? 0 : 1;
}
