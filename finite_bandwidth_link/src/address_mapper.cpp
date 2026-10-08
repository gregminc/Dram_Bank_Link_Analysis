

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

}

AddressMapper::AddressMapper(const AddressMappingConfig& config)
    : layout_{} {
    const std::uint64_t request_size_bytes =
        static_cast<std::uint64_t>(config.bus_width_bytes) *
        config.burst_length;

    if (!IsPowerOfTwo(config.channels) ||
        !IsPowerOfTwo(config.ranks) ||
        !IsPowerOfTwo(config.bank_groups) ||
        !IsPowerOfTwo(config.banks_per_group) ||
        !IsPowerOfTwo(config.rows) ||
        !IsPowerOfTwo(config.columns) ||
        !IsPowerOfTwo(config.burst_length) ||
        !IsPowerOfTwo(request_size_bytes) ||
        config.columns < config.burst_length) {
        throw std::invalid_argument(
            "Address-mapping dimensions must be nonzero powers of two");
    }

    // DRAMsim3 mapping "rochrababgco", read from least to most significant:
    // column, bank group, bank, rank, channel, row.
    layout_.request_offset_bits = LogBase2(request_size_bytes);

    unsigned next_bit = 0;
    const auto AddField = [&next_bit](unsigned width) {
        const BitField field{
            .least_significant_bit = next_bit,
            .width = width,
        };
        next_bit += width;
        return field;
    };

    layout_.column = AddField(
        LogBase2(config.columns) - LogBase2(config.burst_length));
    layout_.bank_group = AddField(LogBase2(config.bank_groups));
    layout_.bank = AddField(LogBase2(config.banks_per_group));
    layout_.rank = AddField(LogBase2(config.ranks));
    layout_.channel = AddField(LogBase2(config.channels));
    layout_.row = AddField(LogBase2(config.rows));

    if (layout_.request_offset_bits + next_bit >
        std::numeric_limits<std::uint64_t>::digits) {
        throw std::invalid_argument("Address mapping requires more than 64 bits");
    }
}

DramAddress AddressMapper::Decode(std::uint64_t physical_address) const {
    const std::uint64_t address =
        physical_address >> layout_.request_offset_bits;

    const auto Extract = [address](const BitField& field) {
        return (address >> field.least_significant_bit) & Mask(field.width);
    };

    DramAddress result{};
    result.column = Extract(layout_.column);
    result.bank_group = static_cast<unsigned>(Extract(layout_.bank_group));
    result.bank = static_cast<unsigned>(Extract(layout_.bank));
    result.rank = static_cast<unsigned>(Extract(layout_.rank));
    result.channel = static_cast<unsigned>(Extract(layout_.channel));
    result.row = Extract(layout_.row);

    return result;
}
