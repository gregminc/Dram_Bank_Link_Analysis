

#include "statistics.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <ostream>
#include <stdexcept>

namespace {

double SafeRatio(double numerator, double denominator) noexcept {
    return denominator > 0.0 ? numerator / denominator : 0.0;
}

std::string EscapeCsv(const std::string& value) {
    if (value.find_first_of(",\"\n") == std::string::npos) {
        return value;
    }

    std::string escaped = "\"";
    for (const char character : value) {
        if (character == '\"') {
            escaped += "\"\"";
        } else {
            escaped += character;
        }
    }
    escaped += '\"';
    return escaped;
}

}

void TimingSamples::Add(double value_ns) {
    ++count;
    sum_ns += value_ns;
    maximum_ns = std::max(maximum_ns, value_ns);
    samples_ns.push_back(value_ns);
}

double TimingSamples::MeanNs() const noexcept {
    return count == 0 ? 0.0 : sum_ns / static_cast<double>(count);
}

double TimingSamples::PercentileNs(double percentile) const {
    if (samples_ns.empty()) {
        return 0.0;
    }
    if (percentile < 0.0 || percentile > 100.0) {
        throw std::invalid_argument("Percentile must be between 0 and 100");
    }

    std::vector<double> sorted = samples_ns;
    std::sort(sorted.begin(), sorted.end());
    const double position =
        (percentile / 100.0) * static_cast<double>(sorted.size() - 1);
    const std::size_t lower = static_cast<std::size_t>(std::floor(position));
    const std::size_t upper = static_cast<std::size_t>(std::ceil(position));
    const double fraction = position - static_cast<double>(lower);
    return sorted[lower] + (sorted[upper] - sorted[lower]) * fraction;
}

SimulationStatistics::SimulationStatistics(
    unsigned number_of_links,
    unsigned number_of_banks
)
    : links_(number_of_links),
      banks_(number_of_banks) {}

void SimulationStatistics::UpdateFifoOccupancy(
    unsigned link_id,
    std::size_t new_occupancy,
    double current_time_ns
) {
    LinkStatistics& link = links_.at(link_id);
    link.fifo_occupancy_area_ns +=
        static_cast<double>(link.current_fifo_occupancy) *
        (current_time_ns - link.last_fifo_change_time_ns);
    link.current_fifo_occupancy = new_occupancy;
    link.maximum_fifo_occupancy =
        std::max(link.maximum_fifo_occupancy, new_occupancy);
    link.last_fifo_change_time_ns = current_time_ns;
}

void SimulationStatistics::RecordAccepted(
    unsigned link_id,
    const LinkRequest& request,
    bool queued,
    std::size_t fifo_occupancy,
    double current_time_ns
) {
    ++accepted_requests_;
    accepted_bits_ += request.transfer_bits;
    if (request.operation == Operation::Read) {
        ++read_requests_;
    } else {
        ++write_requests_;
    }
    if (queued) {
        ++queued_requests_;
    }

    if (!first_ready_time_ns_) {
        first_ready_time_ns_ = request.ready_time_ns;
    }
    last_ready_time_ns_ = request.ready_time_ns;

    LinkStatistics& link = links_.at(link_id);
    ++link.accepted_requests;
    if (queued) {
        ++link.queued_requests;
    }

    BankStatistics& bank = banks_.at(request.bank);
    ++bank.accepted_requests;
    if (queued) {
        ++bank.queued_requests;
    }

    UpdateFifoOccupancy(link_id, fifo_occupancy, current_time_ns);
}

void SimulationStatistics::RecordCompletion(
    const ActiveTransfer& transfer,
    std::size_t fifo_occupancy,
    double current_time_ns
) {
    const double queue_wait_ns =
        transfer.start_time_ns - transfer.request.ready_time_ns;
    const double transfer_time_ns =
        transfer.finish_time_ns - transfer.start_time_ns;
    const double total_latency_ns =
        transfer.finish_time_ns - transfer.request.ready_time_ns;

    ++completed_requests_;
    completed_bits_ += transfer.request.transfer_bits;
    queue_wait_.Add(queue_wait_ns);
    transfer_time_.Add(transfer_time_ns);
    total_latency_.Add(total_latency_ns);

    LinkStatistics& link = links_.at(transfer.link_id);
    ++link.completed_requests;
    link.completed_bits += transfer.request.transfer_bits;
    link.busy_time_ns += transfer_time_ns;
    link.queue_wait.Add(queue_wait_ns);
    link.total_latency.Add(total_latency_ns);

    BankStatistics& bank = banks_.at(transfer.request.bank);
    ++bank.completed_requests;
    bank.completed_bits += transfer.request.transfer_bits;
    bank.queue_wait.Add(queue_wait_ns);
    bank.total_latency.Add(total_latency_ns);

    UpdateFifoOccupancy(
        transfer.link_id,
        fifo_occupancy,
        current_time_ns
    );
}

void SimulationStatistics::RecordOverflow(
    unsigned link_id,
    const LinkRequest& request,
    std::size_t fifo_occupancy,
    double current_time_ns,
    const std::string& reason
) {
    feasible_ = false;
    failure_reason_ = reason;
    overflow_ = OverflowInformation{
        .time_ns = current_time_ns,
        .request_id = request.id,
        .bank = request.bank,
        .link_id = link_id,
    };
    UpdateFifoOccupancy(link_id, fifo_occupancy, current_time_ns);
}

void SimulationStatistics::Finalize(double final_time_ns) {
    if (finalized_) {
        return;
    }
    final_time_ns_ = final_time_ns;
    for (unsigned link_id = 0; link_id < links_.size(); ++link_id) {
        UpdateFifoOccupancy(
            link_id,
            links_[link_id].current_fifo_occupancy,
            final_time_ns
        );
    }
    finalized_ = true;
}

bool SimulationStatistics::IsFeasible() const noexcept { return feasible_; }
const std::string& SimulationStatistics::FailureReason() const noexcept {
    return failure_reason_;
}
const std::optional<OverflowInformation>&
SimulationStatistics::Overflow() const noexcept { return overflow_; }
std::uint64_t SimulationStatistics::AcceptedRequests() const noexcept {
    return accepted_requests_;
}
std::uint64_t SimulationStatistics::CompletedRequests() const noexcept {
    return completed_requests_;
}
std::uint64_t SimulationStatistics::QueuedRequests() const noexcept {
    return queued_requests_;
}
std::uint64_t SimulationStatistics::ReadRequests() const noexcept {
    return read_requests_;
}
std::uint64_t SimulationStatistics::WriteRequests() const noexcept {
    return write_requests_;
}
std::uint64_t SimulationStatistics::AcceptedBits() const noexcept {
    return accepted_bits_;
}
std::uint64_t SimulationStatistics::CompletedBits() const noexcept {
    return completed_bits_;
}

double SimulationStatistics::FirstReadyTimeNs() const noexcept {
    return first_ready_time_ns_.value_or(0.0);
}
double SimulationStatistics::LastReadyTimeNs() const noexcept {
    return last_ready_time_ns_.value_or(0.0);
}
double SimulationStatistics::FinalTimeNs() const noexcept {
    return final_time_ns_;
}
double SimulationStatistics::SimulationDurationNs() const noexcept {
    return first_ready_time_ns_
        ? std::max(0.0, final_time_ns_ - *first_ready_time_ns_)
        : 0.0;
}
double SimulationStatistics::ArrivalSpanNs() const noexcept {
    return first_ready_time_ns_ && last_ready_time_ns_
        ? std::max(0.0, *last_ready_time_ns_ - *first_ready_time_ns_)
        : 0.0;
}
double SimulationStatistics::DrainTailNs() const noexcept {
    return last_ready_time_ns_
        ? std::max(0.0, final_time_ns_ - *last_ready_time_ns_)
        : 0.0;
}
double SimulationStatistics::OfferedBandwidthGbps() const noexcept {
    return SafeRatio(static_cast<double>(accepted_bits_), ArrivalSpanNs());
}
double SimulationStatistics::AchievedThroughputGbps() const noexcept {
    return SafeRatio(static_cast<double>(completed_bits_), SimulationDurationNs());
}
double SimulationStatistics::AverageLinkUtilization() const noexcept {
    if (links_.empty() || SimulationDurationNs() == 0.0) {
        return 0.0;
    }
    double busy_time_ns = 0.0;
    for (const LinkStatistics& link : links_) {
        busy_time_ns += link.busy_time_ns;
    }
    return busy_time_ns /
        (SimulationDurationNs() * static_cast<double>(links_.size()));
}
double SimulationStatistics::MaximumLinkUtilization() const noexcept {
    double maximum = 0.0;
    for (const LinkStatistics& link : links_) {
        maximum = std::max(
            maximum,
            SafeRatio(link.busy_time_ns, SimulationDurationNs())
        );
    }
    return maximum;
}
std::size_t SimulationStatistics::MaximumFifoOccupancy() const noexcept {
    std::size_t maximum = 0;
    for (const LinkStatistics& link : links_) {
        maximum = std::max(maximum, link.maximum_fifo_occupancy);
    }
    return maximum;
}
double SimulationStatistics::AverageFifoOccupancy() const noexcept {
    if (links_.empty() || SimulationDurationNs() == 0.0) {
        return 0.0;
    }
    double total_area_ns = 0.0;
    for (const LinkStatistics& link : links_) {
        total_area_ns += link.fifo_occupancy_area_ns;
    }
    return total_area_ns /
        (SimulationDurationNs() * static_cast<double>(links_.size()));
}

const TimingSamples& SimulationStatistics::QueueWait() const noexcept {
    return queue_wait_;
}
const TimingSamples& SimulationStatistics::TotalLatency() const noexcept {
    return total_latency_;
}
const TimingSamples& SimulationStatistics::TransferTime() const noexcept {
    return transfer_time_;
}
const std::vector<LinkStatistics>&
SimulationStatistics::Links() const noexcept { return links_; }
const std::vector<BankStatistics>&
SimulationStatistics::Banks() const noexcept { return banks_; }

void WriteSummaryCsv(
    const SimulationStatistics& statistics,
    const SimulationConfig& config,
    const std::string& trace_path,
    std::ostream& output
) {
    output
        << "trace,banks_per_link,number_of_links,link_width_bits,"
        << "transfers_per_ns,fifo_depth,transfer_bits,feasible,"
        << "failure_reason,overflow_time_ns,overflow_request_id,"
        << "overflow_bank,overflow_link,accepted_requests,"
        << "completed_requests,queued_requests,read_requests,write_requests,"
        << "accepted_bits,completed_bits,first_ready_time_ns,"
        << "last_ready_time_ns,final_time_ns,simulation_duration_ns,"
        << "arrival_span_ns,drain_tail_ns,offered_bandwidth_gbps,"
        << "achieved_throughput_gbps,mean_transfer_time_ns,"
        << "mean_queue_wait_ns,p95_queue_wait_ns,max_queue_wait_ns,"
        << "mean_total_latency_ns,p95_total_latency_ns,"
        << "max_total_latency_ns,average_link_utilization,"
        << "maximum_link_utilization,average_fifo_occupancy,"
        << "maximum_fifo_occupancy\n";

    const auto& overflow = statistics.Overflow();
    output << std::setprecision(12)
        << EscapeCsv(trace_path) << ','
        << config.link.banks_per_link << ','
        << statistics.Links().size() << ','
        << config.link.width_bits << ','
        << config.link.transfers_per_ns << ','
        << config.link.fifo_depth << ','
        << config.transfer_bits << ','
        << (statistics.IsFeasible() ? "true" : "false") << ','
        << EscapeCsv(statistics.FailureReason()) << ','
        << (overflow ? overflow->time_ns : 0.0) << ','
        << (overflow ? overflow->request_id : 0) << ','
        << (overflow ? overflow->bank : 0) << ','
        << (overflow ? overflow->link_id : 0) << ','
        << statistics.AcceptedRequests() << ','
        << statistics.CompletedRequests() << ','
        << statistics.QueuedRequests() << ','
        << statistics.ReadRequests() << ','
        << statistics.WriteRequests() << ','
        << statistics.AcceptedBits() << ','
        << statistics.CompletedBits() << ','
        << statistics.FirstReadyTimeNs() << ','
        << statistics.LastReadyTimeNs() << ','
        << statistics.FinalTimeNs() << ','
        << statistics.SimulationDurationNs() << ','
        << statistics.ArrivalSpanNs() << ','
        << statistics.DrainTailNs() << ','
        << statistics.OfferedBandwidthGbps() << ','
        << statistics.AchievedThroughputGbps() << ','
        << statistics.TransferTime().MeanNs() << ','
        << statistics.QueueWait().MeanNs() << ','
        << statistics.QueueWait().PercentileNs(95.0) << ','
        << statistics.QueueWait().maximum_ns << ','
        << statistics.TotalLatency().MeanNs() << ','
        << statistics.TotalLatency().PercentileNs(95.0) << ','
        << statistics.TotalLatency().maximum_ns << ','
        << statistics.AverageLinkUtilization() << ','
        << statistics.MaximumLinkUtilization() << ','
        << statistics.AverageFifoOccupancy() << ','
        << statistics.MaximumFifoOccupancy() << '\n';
}

void WriteLinkStatisticsCsv(
    const SimulationStatistics& statistics,
    std::ostream& output
) {
    output
        << "link_id,accepted_requests,completed_requests,queued_requests,"
        << "completed_bits,busy_time_ns,utilization,average_fifo_occupancy,"
        << "maximum_fifo_occupancy,mean_queue_wait_ns,p95_queue_wait_ns,"
        << "max_queue_wait_ns,mean_total_latency_ns,p95_total_latency_ns,"
        << "max_total_latency_ns\n";

    const double duration_ns = statistics.SimulationDurationNs();
    const auto& links = statistics.Links();
    output << std::setprecision(12);
    for (std::size_t link_id = 0; link_id < links.size(); ++link_id) {
        const LinkStatistics& link = links[link_id];
        output
            << link_id << ','
            << link.accepted_requests << ','
            << link.completed_requests << ','
            << link.queued_requests << ','
            << link.completed_bits << ','
            << link.busy_time_ns << ','
            << SafeRatio(link.busy_time_ns, duration_ns) << ','
            << SafeRatio(link.fifo_occupancy_area_ns, duration_ns) << ','
            << link.maximum_fifo_occupancy << ','
            << link.queue_wait.MeanNs() << ','
            << link.queue_wait.PercentileNs(95.0) << ','
            << link.queue_wait.maximum_ns << ','
            << link.total_latency.MeanNs() << ','
            << link.total_latency.PercentileNs(95.0) << ','
            << link.total_latency.maximum_ns << '\n';
    }
}

void WriteBankStatisticsCsv(
    const SimulationStatistics& statistics,
    std::ostream& output
) {
    output
        << "bank,accepted_requests,completed_requests,queued_requests,"
        << "completed_bits,mean_queue_wait_ns,p95_queue_wait_ns,"
        << "max_queue_wait_ns,mean_total_latency_ns,p95_total_latency_ns,"
        << "max_total_latency_ns\n";

    const auto& banks = statistics.Banks();
    output << std::setprecision(12);
    for (std::size_t bank_id = 0; bank_id < banks.size(); ++bank_id) {
        const BankStatistics& bank = banks[bank_id];
        output
            << bank_id << ','
            << bank.accepted_requests << ','
            << bank.completed_requests << ','
            << bank.queued_requests << ','
            << bank.completed_bits << ','
            << bank.queue_wait.MeanNs() << ','
            << bank.queue_wait.PercentileNs(95.0) << ','
            << bank.queue_wait.maximum_ns << ','
            << bank.total_latency.MeanNs() << ','
            << bank.total_latency.PercentileNs(95.0) << ','
            << bank.total_latency.maximum_ns << '\n';
    }
}
