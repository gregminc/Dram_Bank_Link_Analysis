

#pragma once

#include "simulation_config.hpp"

#include <cstdint>

struct DramAddress {
    unsigned channel;
    unsigned rank;
    unsigned bank_group;
    unsigned bank;
    std::uint64_t row;
    std::uint64_t column;
};

class AddressMapper {
public:
    explicit AddressMapper(const AddressMappingConfig& config);

    DramAddress Decode(std::uint64_t physical_address) const;

private:
    struct BitField {
        unsigned least_significant_bit = 0;
        unsigned width = 0;
    };

    struct AddressLayout {
        unsigned request_offset_bits = 0;
        BitField column;
        BitField bank_group;
        BitField bank;
        BitField rank;
        BitField channel;
        BitField row;
    };

    AddressLayout layout_;
};
