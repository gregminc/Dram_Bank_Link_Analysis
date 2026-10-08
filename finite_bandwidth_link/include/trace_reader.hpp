

#pragma once

#include "simulation_types.hpp"

#include <cstdint>
#include <fstream>
#include <string>

struct TraceRecord {
    double ready_time_ns;
    Operation operation;
    std::uint64_t address;
};

class TraceReader {
public:
    explicit TraceReader(const std::string& filename);
    bool Next(TraceRecord& record);
    bool IsOpen() const;
private:
    std::ifstream input_;
    std::size_t line_number_ = 0;
};