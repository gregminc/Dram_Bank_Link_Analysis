

#pragma once

#include <cstddef>

struct AddressMappingConfig {
    unsigned channels = 1;
    unsigned ranks = 1;
    unsigned bank_groups = 2;
    unsigned banks_per_group = 4;
    unsigned rows = 65536;
    unsigned columns = 1024;
    unsigned bus_width_bytes = 8;
    unsigned burst_length = 16;
};

struct LinkConfig {
    unsigned banks_per_link = 2;
    unsigned width_bits = 16;
    double transfers_per_ns = 4.266;
    std::size_t fifo_depth = 4;
};

struct SimulationConfig {
    AddressMappingConfig address_mapping{};
    LinkConfig link{};
    unsigned transfer_bits = 256;

    [[nodiscard]] constexpr unsigned BanksPerRank() const noexcept {
        return address_mapping.bank_groups *
               address_mapping.banks_per_group;
    }

    [[nodiscard]] constexpr unsigned TotalBanks() const noexcept {
        return address_mapping.channels *
               address_mapping.ranks *
               BanksPerRank();
    }
};

inline constexpr SimulationConfig kDefaultSimulationConfig{};
