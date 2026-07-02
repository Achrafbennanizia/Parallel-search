#pragma once

#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

struct TestRecord {
    std::string name;
    std::string status;
    double durationMs = 0.0;
};

struct BenchmarkRecord {
    std::string name;
    double durationMs = 0.0;
    double speedup = -1.0;
    int networksFound = -1;
    int optimum = -1;
    std::string detail;
};

class TestReport {
public:
    void addTest(const TestRecord& record) { tests_.push_back(record); }
    void addBenchmark(const BenchmarkRecord& record) { benchmarks_.push_back(record); }

    bool writeTestsCsv(const std::string& path, const std::string& timestamp, const std::string& build,
                       unsigned threads) const;

    bool writeBenchmarksCsv(const std::string& path, const std::string& timestamp, const std::string& build,
                            unsigned threads) const;

    std::size_t testCount() const { return tests_.size(); }
    std::size_t benchmarkCount() const { return benchmarks_.size(); }

private:
    static std::string csvEscape(const std::string& value);
    static std::string formatDouble(double value, int precision = 6);

    std::vector<TestRecord> tests_;
    std::vector<BenchmarkRecord> benchmarks_;
};

std::string currentTimestamp();
