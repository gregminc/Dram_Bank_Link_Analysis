

#include "statistics.hpp"

#include <algorithm>
#include <fstream>
#include <stdexcept>
#include <vector>

Statistics::Statistics(const AnalysisConfig& config)
    : num_banks_(config.channels * config.ranks_per_channel *
                 config.bank_groups_per_rank * config.banks_per_group),
      config_(config),
      bank_statistics_(num_banks_) {

    if (config.channels == 0 ||
        config.ranks_per_channel == 0 ||
        config.bank_groups_per_rank == 0 ||
        config.banks_per_group == 0 ||
        config.banks_per_link_group == 0 ||
        num_banks_ % config.banks_per_link_group != 0 ||
        config.window_size_ns <= 0) {

        throw std::invalid_argument("Invalid Analysis Configuration");
    }
}

unsigned Statistics::BankIndex(const DramAddress& dram_address) const {
    if (dram_address.channel >= config_.channels ||
        dram_address.rank >= config_.ranks_per_channel ||
        dram_address.bank_group >= config_.bank_groups_per_rank ||
        dram_address.bank >= config_.banks_per_group) {
        throw std::invalid_argument("Invalid DRAM address");
    }

    return (((dram_address.channel * config_.ranks_per_channel +
              dram_address.rank) * config_.bank_groups_per_rank +
             dram_address.bank_group) * config_.banks_per_group +
            dram_address.bank);
}

unsigned Statistics::LinkGroupForBank(unsigned bank_index) const {
    return bank_index / config_.banks_per_link_group;
}

void Statistics::Observe(const TraceRecord& request, const DramAddress& dram_address) {
    const unsigned bank_index = BankIndex(dram_address);

    total_requests_++;

    if (request.operation == Operation::Read) {
        bank_statistics_[bank_index].reads++;
    } else {
        bank_statistics_[bank_index].writes++;
    }

    std::uint64_t window = request.time_stamp_ns / config_.window_size_ns;

    auto& counts = window_bank_counts_[window];
    if (counts.empty()) {
        counts.resize(num_banks_);
    }
    counts[bank_index]++;
}

void Statistics::WriteBankStatistics(const std::string& filename) const {
    std::ofstream output(filename);

    if (!output.is_open()) {
        throw std::runtime_error("Failed to open file for writing: " + filename);
    }

    output << "bank,reads,writes,total,fraction\n";

    for (std::size_t idx = 0; idx < bank_statistics_.size(); ++idx) {
        const auto& stats = bank_statistics_[idx];
        const std::uint64_t bank_total = stats.reads + stats.writes;
        const double fraction = total_requests_ == 0
            ? 0.0
            : static_cast<double>(bank_total) / total_requests_;

        output << idx                        << ','
               << stats.reads                << ','
               << stats.writes               << ','
               << bank_total                 << ','
               << fraction                   << '\n';
    }
}

void Statistics::WriteActiveBankHistogram(const std::string& filename) const {
    std::vector<std::uint64_t> histogram(num_banks_ + 1 , 0);

    int active_banks{0};
    for (auto [window, banks_active] : window_bank_counts_) {
        for (auto bank_count : banks_active) {
            if (bank_count) active_banks++;
        }
        histogram[active_banks]++;
        active_banks = 0;
    }

    std::ofstream output(filename);
    if (!output.is_open()) {
        throw std::runtime_error("Failed to open file for writing: " + filename);
    }

    output << "active_banks,window_count\n";

    for (std::size_t idx = 0; idx < histogram.size(); ++idx) {
        output << idx << ',' << histogram[idx] << '\n';
    }
}

void Statistics::WriteGroupStatistics(const std::string& filename) const {
    struct GroupSummary {
        std::uint64_t total_requests{0};
        std::uint64_t active_windows{0};
        std::uint64_t contended_windows{0};
        std::uint64_t maximum_requests_in_window{0};
    };

    const unsigned num_groups = num_banks_ / config_.banks_per_link_group;
    std::vector<GroupSummary> summaries(num_groups);

    for (auto [window, bank_counts] : window_bank_counts_) {
        std::vector<std::uint64_t> group_counts(num_groups);

        for (std::size_t bank = 0; bank < bank_counts.size(); ++bank) {
            group_counts[LinkGroupForBank(bank)] += bank_counts[bank];
        }

        for (unsigned group = 0; group < num_groups; ++group) {
            summaries[group].total_requests += group_counts[group];

            if (group_counts[group]) {
                summaries[group].active_windows++;
            }
            if (group_counts[group] > 1) {
                summaries[group].contended_windows++;
            }

            summaries[group].maximum_requests_in_window
                = std::max(summaries[group].maximum_requests_in_window, group_counts[group]);
        }
    }

    std::ofstream output(filename);

    if (!output.is_open()) {
        throw std::runtime_error("Failed to open file for writing: " + filename);
    }

    output << "group,total_requests,active_windows,contended_windows,"
           << "contention_fraction,max_requests_in_window\n";

    for (unsigned group = 0; group < num_groups; ++group) {
        GroupSummary summary = summaries[group];
        const double contention_fraction = summary.active_windows == 0
            ? 0.0
            : static_cast<double>(summary.contended_windows)
                / summary.active_windows;
        output << group << ','
               << summary.total_requests             << ','
               << summary.active_windows             << ','
               << summary.contended_windows          << ','
               << contention_fraction                << ','
               << summary.maximum_requests_in_window << '\n';
    }
}
