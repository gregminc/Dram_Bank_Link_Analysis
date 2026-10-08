

#pragma once

#include "address_mapper.hpp"
#include "trace_reader.hpp"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

struct AnalysisConfig {
    unsigned channels;
    unsigned ranks_per_channel;
    unsigned bank_groups_per_rank;
    unsigned banks_per_group;
    unsigned banks_per_link_group;
    double window_size_ns;
};

struct BankStatistics {
    std::uint64_t reads = 0;
    std::uint64_t writes = 0;
};

class Statistics {
public:
    explicit Statistics(const AnalysisConfig& config);

    void Observe(const TraceRecord& request, const DramAddress& dram_address);
    void WriteBankStatistics(const std::string& filename) const;
    void WriteActiveBankHistogram(const std::string& filename) const;
    void WriteGroupStatistics(const std::string& filename) const;

private:
    std::uint64_t total_requests_ = 0;
    unsigned num_banks_;
    AnalysisConfig config_;
    std::vector<BankStatistics> bank_statistics_;
    std::unordered_map<std::uint64_t, std::vector<uint64_t>>
        window_bank_counts_;

    unsigned BankIndex(const DramAddress& dram_address) const;
    unsigned LinkGroupForBank(unsigned bank_index) const;
};
