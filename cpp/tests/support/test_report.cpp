#include "test_report.hpp"

#include <ctime>
#include <filesystem>
#include <fstream>

namespace {

bool fileNeedsHeader(const std::string& path) {
    if (!std::filesystem::exists(path)) {
        return true;
    }
    return std::filesystem::file_size(path) == 0;
}

bool ensureParentDirectory(const std::string& path) {
    const std::filesystem::path parent = std::filesystem::path(path).parent_path();
    if (parent.empty()) {
        return true;
    }
    std::error_code error;
    std::filesystem::create_directories(parent, error);
    return !error;
}

}  // namespace

std::string currentTimestamp() {
    const std::time_t now = std::time(nullptr);
    std::tm localTime{};
#if defined(_WIN32)
    localtime_s(&localTime, &now);
#else
    localtime_r(&now, &localTime);
#endif

    std::ostringstream out;
    out << std::put_time(&localTime, "%Y-%m-%d %H:%M:%S");
    return out.str();
}

std::string TestReport::csvEscape(const std::string& value) {
    if (value.find_first_of(",\"\n") == std::string::npos) {
        return value;
    }

    std::string escaped = "\"";
    for (const char character : value) {
        if (character == '"') {
            escaped += "\"\"";
        } else {
            escaped += character;
        }
    }
    escaped += '"';
    return escaped;
}

std::string TestReport::formatDouble(double value, int precision) {
    std::ostringstream out;
    out << std::fixed << std::setprecision(precision) << value;
    return out.str();
}

bool TestReport::writeTestsCsv(const std::string& path, const std::string& timestamp,
                               const std::string& build, unsigned threads) const {
    if (!ensureParentDirectory(path)) {
        return false;
    }

    const bool writeHeader = fileNeedsHeader(path);
    std::ofstream file(path, std::ios::app);
    if (!file) {
        return false;
    }

    if (writeHeader) {
        file << "timestamp,build,threads,test_name,status,duration_ms\n";
    }

    for (const TestRecord& record : tests_) {
        file << csvEscape(timestamp) << ',' << csvEscape(build) << ',' << threads << ','
             << csvEscape(record.name) << ',' << csvEscape(record.status) << ','
             << formatDouble(record.durationMs) << '\n';
    }

    return static_cast<bool>(file);
}

bool TestReport::writeBenchmarksCsv(const std::string& path, const std::string& timestamp,
                                    const std::string& build, unsigned threads) const {
    if (!ensureParentDirectory(path)) {
        return false;
    }

    const bool writeHeader = fileNeedsHeader(path);
    std::ofstream file(path, std::ios::app);
    if (!file) {
        return false;
    }

    if (writeHeader) {
        file << "timestamp,build,threads,benchmark_name,duration_ms,speedup,networks_found,optimum,detail\n";
    }

    for (const BenchmarkRecord& record : benchmarks_) {
        file << csvEscape(timestamp) << ',' << csvEscape(build) << ',' << threads << ','
             << csvEscape(record.name) << ',' << formatDouble(record.durationMs) << ',';

        if (record.speedup >= 0.0) {
            file << formatDouble(record.speedup, 4);
        }
        file << ',';

        if (record.networksFound >= 0) {
            file << record.networksFound;
        }
        file << ',';

        if (record.optimum >= 0) {
            file << record.optimum;
        }
        file << ',' << csvEscape(record.detail) << '\n';
    }

    return static_cast<bool>(file);
}
