

#include "address_mapper.hpp"

#include <limits>
#include <stdexcept>

namespace {

bool IsPowerOfTwo(std::uint64_t value) {
    return value != 0 && (value & (value - 1)) == 0;
}

unsigned LogBase2(std::uint64_t value) {
    unsigned result = 0;
    while (value > 1) {
        value >>= 1;
        ++result;
    }
    return result;
}

std::uint64_t Mask(unsigned width) {
    if (width == 0) {
        return 0;
    }
    if (width == std::numeric_limits<std::uint64_t>::digits) {
        return std::numeric_limits<std::uint64_t>::max();
    }
    return (std::uint64_t{1} << width) - 1;
}

}  // namespace

AddressMapper::AddressMapper(const AddressMappingConfig& config)
    : config_(config) {
    const std::uint64_t request_size_bytes =
        static_cast<std::uint64_t>(config_.bus_width_bytes) *
        config_.burst_length;

    if (!IsPowerOfTwo(config_.channels) ||
        !IsPowerOfTwo(config_.ranks) ||
        !IsPowerOfTwo(config_.bank_groups) ||
        !IsPowerOfTwo(config_.banks_per_group) ||
        !IsPowerOfTwo(config_.rows) ||
        !IsPowerOfTwo(config_.columns) ||
        !IsPowerOfTwo(config_.burst_length) ||
        !IsPowerOfTwo(request_size_bytes) ||
        config_.columns < config_.burst_length) {
        throw std::invalid_argument(
            "Address-mapping dimensions must be nonzero powers of two");
    }

    request_offset_bits_ = LogBase2(request_size_bytes);
    column_bits_ = LogBase2(config_.columns) - LogBase2(config_.burst_length);
    bank_group_bits_ = LogBase2(config_.bank_groups);
    bank_bits_ = LogBase2(config_.banks_per_group);
    rank_bits_ = LogBase2(config_.ranks);
    channel_bits_ = LogBase2(config_.channels);
    row_bits_ = LogBase2(config_.rows);

    // DRAMsim3 mapping "rochrababgco", read from least to most significant:
    // column, bank group, bank, rank, channel, row.
    column_position_ = 0;
    bank_group_position_ = column_position_ + column_bits_;
    bank_position_ = bank_group_position_ + bank_group_bits_;
    rank_position_ = bank_position_ + bank_bits_;
    channel_position_ = rank_position_ + rank_bits_;
    row_position_ = channel_position_ + channel_bits_;

    if (request_offset_bits_ + row_position_ + row_bits_ >
        std::numeric_limits<std::uint64_t>::digits) {
        throw std::invalid_argument("Address mapping requires more than 64 bits");
    }
}

DramAddress AddressMapper::Decode(std::uint64_t physical_address) const {
    const std::uint64_t address = physical_address >> request_offset_bits_;

    DramAddress result{};
    result.column = (address >> column_position_) & Mask(column_bits_);
    result.bank_group =
        (address >> bank_group_position_) & Mask(bank_group_bits_);
    result.bank = (address >> bank_position_) & Mask(bank_bits_);
    result.rank = (address >> rank_position_) & Mask(rank_bits_);
    result.channel = (address >> channel_position_) & Mask(channel_bits_);
    result.row = (address >> row_position_) & Mask(row_bits_);

    return result;
}
