#pragma once

#include "link.hpp"
#include "simulation_config.hpp"
#include "simulation_types.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <ostream>
#include <string>
#include <vector>

struct TimingSamples {
    std::uint64_t count = 0;
    double sum_ns = 0.0;
    double maximum_ns = 0.0;
    std::vector<double> samples_ns;

    void Add(double value_ns);
    [[nodiscard]] double MeanNs() const noexcept;
    [[nodiscard]] double PercentileNs(double percentile) const;
};

struct LinkStatistics {
    std::uint64_t accepted_requests = 0;
    std::uint64_t completed_requests = 0;
    std::uint64_t queued_requests = 0;
    std::uint64_t completed_bits = 0;
    double busy_time_ns = 0.0;

    std::size_t current_fifo_occupancy = 0;
    std::size_t maximum_fifo_occupancy = 0;
    double fifo_occupancy_area_ns = 0.0;
    double last_fifo_change_time_ns = 0.0;

    TimingSamples queue_wait;
    TimingSamples total_latency;
};

struct BankStatistics {
    std::uint64_t accepted_requests = 0;
    std::uint64_t completed_requests = 0;
    std::uint64_t queued_requests = 0;
    std::uint64_t completed_bits = 0;
    TimingSamples queue_wait;
    TimingSamples total_latency;
};

struct OverflowInformation {
    double time_ns = 0.0;
    std::uint64_t request_id = 0;
    unsigned bank = 0;
    unsigned link_id = 0;
};

class SimulationStatistics {
public:
    SimulationStatistics(unsigned number_of_links, unsigned number_of_banks);

    void RecordAccepted(
        unsigned link_id,
        const LinkRequest& request,
        bool queued,
        std::size_t fifo_occupancy,
        double current_time_ns
    );
    void RecordCompletion(
        const ActiveTransfer& transfer,
        std::size_t fifo_occupancy,
        double current_time_ns
    );
    void RecordOverflow(
        unsigned link_id,
        const LinkRequest& request,
        std::size_t fifo_occupancy,
        double current_time_ns,
        const std::string& reason
    );
    void Finalize(double final_time_ns);

    [[nodiscard]] bool IsFeasible() const noexcept;
    [[nodiscard]] const std::string& FailureReason() const noexcept;
    [[nodiscard]] const std::optional<OverflowInformation>& Overflow() const noexcept;

    [[nodiscard]] std::uint64_t AcceptedRequests() const noexcept;
    [[nodiscard]] std::uint64_t CompletedRequests() const noexcept;
    [[nodiscard]] std::uint64_t QueuedRequests() const noexcept;
    [[nodiscard]] std::uint64_t ReadRequests() const noexcept;
    [[nodiscard]] std::uint64_t WriteRequests() const noexcept;
    [[nodiscard]] std::uint64_t AcceptedBits() const noexcept;
    [[nodiscard]] std::uint64_t CompletedBits() const noexcept;

    [[nodiscard]] double FirstReadyTimeNs() const noexcept;
    [[nodiscard]] double LastReadyTimeNs() const noexcept;
    [[nodiscard]] double FinalTimeNs() const noexcept;
    [[nodiscard]] double SimulationDurationNs() const noexcept;
    [[nodiscard]] double ArrivalSpanNs() const noexcept;
    [[nodiscard]] double DrainTailNs() const noexcept;
    [[nodiscard]] double OfferedBandwidthGbps() const noexcept;
    [[nodiscard]] double AchievedThroughputGbps() const noexcept;
    [[nodiscard]] double AverageLinkUtilization() const noexcept;
    [[nodiscard]] double MaximumLinkUtilization() const noexcept;
    [[nodiscard]] std::size_t MaximumFifoOccupancy() const noexcept;
    [[nodiscard]] double AverageFifoOccupancy() const noexcept;

    [[nodiscard]] const TimingSamples& QueueWait() const noexcept;
    [[nodiscard]] const TimingSamples& TotalLatency() const noexcept;
    [[nodiscard]] const TimingSamples& TransferTime() const noexcept;
    [[nodiscard]] const std::vector<LinkStatistics>& Links() const noexcept;
    [[nodiscard]] const std::vector<BankStatistics>& Banks() const noexcept;

private:
    void UpdateFifoOccupancy(
        unsigned link_id,
        std::size_t new_occupancy,
        double current_time_ns
    );

    bool feasible_ = true;
    bool finalized_ = false;
    std::string failure_reason_;
    std::optional<OverflowInformation> overflow_;

    std::uint64_t accepted_requests_ = 0;
    std::uint64_t completed_requests_ = 0;
    std::uint64_t queued_requests_ = 0;
    std::uint64_t read_requests_ = 0;
    std::uint64_t write_requests_ = 0;
    std::uint64_t accepted_bits_ = 0;
    std::uint64_t completed_bits_ = 0;

    std::optional<double> first_ready_time_ns_;
    std::optional<double> last_ready_time_ns_;
    double final_time_ns_ = 0.0;

    TimingSamples queue_wait_;
    TimingSamples total_latency_;
    TimingSamples transfer_time_;
    std::vector<LinkStatistics> links_;
    std::vector<BankStatistics> banks_;
};

void WriteSummaryCsv(
    const SimulationStatistics& statistics,
    const SimulationConfig& config,
    const std::string& trace_path,
    std::ostream& output
);

void WriteLinkStatisticsCsv(
    const SimulationStatistics& statistics,
    std::ostream& output
);

void WriteBankStatisticsCsv(
    const SimulationStatistics& statistics,
    std::ostream& output
);
