

#pragma once

#include <optional>
#include <vector>

#include "bank_link_mapper.hpp"
#include "link.hpp"
#include "simulation_config.hpp"
#include "simulation_types.hpp"
#include "statistics.hpp"

#include <stdexcept>

class QueueOverflowError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

class RequestBuilder;
class TraceReader;

class Simulator {
public: 
    explicit Simulator(const SimulationConfig& config);

    void Submit(const LinkRequest& request);
    void Run(TraceReader& trace_reader, RequestBuilder& request_builder);

    [[nodiscard]] const SimulationStatistics& Statistics() const noexcept;

private:
    [[nodiscard]] std::optional<double> NextTime() const;
    void BufferNextRequest(
        TraceReader& trace_reader,
        RequestBuilder& request_builder
    );

    std::optional<LinkRequest> next_request_;
    double current_time_ns_ = 0.0;

    SimulationConfig config_;
    BankLinkMapper mapper_;
    std::vector<Link> links_;
    SimulationStatistics statistics_;
};
