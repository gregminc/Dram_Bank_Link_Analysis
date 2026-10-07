

#pragma once

#include <cstdint>
#include <fstream>
#include <string>

enum class Operation {
    Read, Write
};

struct TraceRecord {
    double time_stamp_ns;
    Operation operation;
    std::uint64_t address;
};

class TraceReader {
public:
    explicit TraceReader(const std::string& filename);

    bool IsOpen() const;
    bool Next(TraceRecord& record);

private:
    std::ifstream input_;
    std::size_t line_number_ = 0;
};