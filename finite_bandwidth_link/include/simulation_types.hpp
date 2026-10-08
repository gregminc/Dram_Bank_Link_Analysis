

#pragma once

#include <cstdint>

enum class Operation {
    Read, Write
};

struct LinkRequest {
    std::uint64_t id = 0;
    double ready_time_ns = 0.0;
    unsigned bank = 0;
    Operation operation = Operation::Read;
    unsigned transfer_bits = 256;
};