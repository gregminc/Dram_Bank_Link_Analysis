

#pragma once

#include <cstdint>

struct DramAddress {
    unsigned channel;
    unsigned rank;
    unsigned bank_group;
    unsigned bank;
    std::uint64_t row;
    std::uint64_t column;
};

struct AddressMappingConfig {
    unsigned channels;
    unsigned ranks;
    unsigned bank_groups;
    unsigned banks_per_group;
    unsigned rows;
    unsigned columns;
    unsigned bus_width_bytes;
    unsigned burst_length;
};

class AddressMapper {
public:
    explicit AddressMapper(const AddressMappingConfig& config);

    DramAddress Decode(std::uint64_t physical_address) const;

private:
    AddressMappingConfig config_;

    unsigned request_offset_bits_;
    unsigned column_bits_;
    unsigned bank_group_bits_;
    unsigned bank_bits_;
    unsigned rank_bits_;
    unsigned channel_bits_;
    unsigned row_bits_;

    unsigned column_position_;
    unsigned bank_group_position_;
    unsigned bank_position_;
    unsigned rank_position_;
    unsigned channel_position_;
    unsigned row_position_;
};
