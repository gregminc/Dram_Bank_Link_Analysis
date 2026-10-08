

#include "simulator.hpp"
#include "link.hpp"
#include "request_builder.hpp"
#include "trace_reader.hpp"

#include <stdexcept>
#include <string>

Simulator::Simulator(const SimulationConfig& config) 
    : config_(config),
      mapper_(config.TotalBanks(), config.link.banks_per_link),
      links_(),
      statistics_(mapper_.NumberOfLinks(), config.TotalBanks()) {
    links_.reserve(mapper_.NumberOfLinks());
    for (unsigned link_id = 0; link_id < mapper_.NumberOfLinks(); ++link_id) {
        links_.emplace_back(
            link_id,
            config_.link.width_bits,
            config_.link.transfers_per_ns,
            config_.link.fifo_depth
        );
    }
}

std::optional<double> Simulator::NextTime() const {
    std::optional<double> next_time;

    if (next_request_) {
        next_time = next_request_->ready_time_ns;
    }

    for (const Link& link : links_) {
        const std::optional<double> completion_time = link.CompletionTimeNs();
        if (completion_time && (!next_time || *completion_time < *next_time)) {
            next_time = *completion_time;
        }
    }

    return next_time;
}

void Simulator::Submit(const LinkRequest& request) {
    const unsigned link_id = mapper_.LinkForBank(request.bank);
    Link& link = links_[link_id];
    const bool queued = link.IsBusy();

    if (!link.Accept(request)) {
        const std::string message =
            "Link queue overflow for request " + std::to_string(request.id) +
            " (bank " + std::to_string(request.bank) +
            ", link " + std::to_string(link_id) +
            ", ready at " + std::to_string(request.ready_time_ns) + " ns)";
        statistics_.RecordOverflow(
            link_id,
            request,
            link.WaitingRequestCount(),
            current_time_ns_,
            message
        );
        statistics_.Finalize(current_time_ns_);
        throw QueueOverflowError(message);
    }

    statistics_.RecordAccepted(
        link_id,
        request,
        queued,
        link.WaitingRequestCount(),
        current_time_ns_
    );
}

void Simulator::BufferNextRequest(
    TraceReader& trace_reader,
    RequestBuilder& request_builder
) {
    TraceRecord record{};
    if (!trace_reader.Next(record)) {
        next_request_.reset();
        return;
    }

    LinkRequest request = request_builder.Build(record);
    if (request.ready_time_ns < current_time_ns_) {
        throw std::runtime_error(
            "Trace requests must be ordered by nondecreasing ready time"
        );
    }

    next_request_ = request;
}

void Simulator::Run(
    TraceReader& trace_reader,
    RequestBuilder& request_builder
) {
    BufferNextRequest(trace_reader, request_builder);

    while (const std::optional<double> next_time = NextTime()) {
        current_time_ns_ = *next_time;

        for (Link& link : links_) {
            const std::optional<double> completion_time =
                link.CompletionTimeNs();
            if (completion_time && *completion_time <= current_time_ns_) {
                const ActiveTransfer completed = link.CompleteTransfer();
                statistics_.RecordCompletion(
                    completed,
                    link.WaitingRequestCount(),
                    current_time_ns_
                );
            }
        }

        while (next_request_ &&
               next_request_->ready_time_ns <= current_time_ns_) {
            const LinkRequest request = *next_request_;
            next_request_.reset();
            Submit(request);
            BufferNextRequest(trace_reader, request_builder);
        }
    }

    statistics_.Finalize(current_time_ns_);
}

const SimulationStatistics& Simulator::Statistics() const noexcept {
    return statistics_;
}
